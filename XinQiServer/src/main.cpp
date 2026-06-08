/**
 * XinQi (芯棋) HTTP 服务端
 *
 * 编译: MSBuild XinQiServer.vcxproj /p:Configuration=Debug /p:Platform=x64
 * 运行: XinQiServer\x64\Debug\XinQiServer.exe
 *
 * 依赖: XinQiCore.lib, httplib.h, json.hpp
 */

#include <XinQiCore.h>
#include <XinQiAI.h>
#include <httplib.h>
#include <json.hpp>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <fstream>
#include <ctime>
#include <filesystem>
#include <cstring>
#include <cstdio>
namespace fs = std::filesystem;

using json = nlohmann::json;

// ── 全局游戏状态 ──────────────────────────────────────────

static GameState* g_gs = nullptr;
static bool g_terminated = false;
static std::string g_winner = "None";
static std::vector<json> g_moves; // 走法历史

// ── JSON 工具 ─────────────────────────────────────────────

static json boardToJson(const GameState* gs)
{
    int N = gs->size;
    int total = N * N * N;
    const int8_t* board = XinQi_BoardData(gs);
    json arr = json::array();
    for (int i = 0; i < total; ++i) {
        arr.push_back((int)board[i]);
    }
    return arr;
}

static json stateToJson(const GameState* gs)
{
    json j;
    j["board_size"] = gs->size;
    j["board"] = boardToJson(gs);
    j["current_player"] = gs->current == COLOR_BLACK ? "Black" : "White";
    j["move_count"] = gs->moveCount;
    j["terminal"] = g_terminated;
    if (g_terminated) {
      j["winner"] = g_winner;
    }

    // 内芯计数
    const int8_t* board = XinQi_BoardData(gs);
    int N = gs->size;
    int total = N * N * N;
    int blackCores = 0, whiteCores = 0;
    for (int i = 0; i < total; ++i) {
        if (board[i] != CELL_BLACK && board[i] != CELL_WHITE) continue;
        int8_t x = (int8_t)(i / (N * N));
        int8_t y = (int8_t)((i / N) % N);
        int8_t z = (int8_t)(i % N);
        if (board[i] == CELL_BLACK && XinQi_IsCore(gs, x, y, z)) blackCores++;
        if (board[i] == CELL_WHITE && XinQi_IsCore(gs, x, y, z)) whiteCores++;
    }
    j["inner_core_count"]["Black"] = blackCores;
    j["inner_core_count"]["White"] = whiteCores;

    // 内芯空位
    json vac = json::array();
    json vacOwners = json::object();
    for (int i = 0; i < total; ++i) {
        if (board[i] == CELL_BLACK_VACANCY) {
            vac.push_back(i);
            vacOwners[std::to_string(i)] = "Black";
        } else if (board[i] == CELL_WHITE_VACANCY) {
            vac.push_back(i);
            vacOwners[std::to_string(i)] = "White";
        }
    }
    j["vacancy_indices"] = vac;
    j["vacancy_owners"] = vacOwners;

    return j;
}

/** 十六进制字符串 → vector<uint8_t> */
static std::vector<uint8_t> hex_to_bytes(const std::string& hex)
{
    std::vector<uint8_t> data;
    data.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        auto h = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return 0;
        };
        data.push_back((h(hex[i]) << 4) | h(hex[i + 1]));
    }
    return data;
}

/** 字节数组 → 十六进制字符串 */
static std::string bytes_to_hex(const uint8_t* data, size_t len)
{
    std::string hex;
    hex.reserve(len * 2);
    const char* tbl = "0123456789abcdef";
    for (size_t i = 0; i < len; ++i) {
        hex += tbl[data[i] >> 4];
        hex += tbl[data[i] & 0xf];
    }
    return hex;
}

// ── CORS 中间件 ───────────────────────────────────────────

static void addCors(httplib::Response& res)
{
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    res.set_header("Access-Control-Allow-Headers", "Content-Type");
}

// ── 辅助：结果码转为字符串 ───────────────────────────────

static json resultToJson(int8_t r, GameState* gs, int8_t x, int8_t y, int8_t z)
{
    json j;
    if (r >= 1) {
        j["legal"] = true;
        j["terminal"] = true;
        j["result_code"] = r;
        // gs->current 是败方（Place/Shift 已切为 Opp(winner)）
        j["winner"] = (gs->current == COLOR_BLACK) ? "White" : "Black";
        j["next_player"] = (gs->current == COLOR_BLACK) ? "Black" : "White";
        j["captured"] = json::array();
        j["captured_count"] = gs->lastCaptureCount;
    } else if (r == RESULT_OK) {
        j["legal"] = true;
        j["result_code"] = 0;
        j["terminal"] = !XinQi_HasAnyLegalMove(gs);
        j["next_player"] = (gs->current == COLOR_BLACK) ? "Black" : "White";
        j["captured"] = json::array();
        j["captured_count"] = gs->lastCaptureCount;
        if (j["terminal"].get<bool>()) {
            j["winner"] = (gs->current == COLOR_BLACK) ? "White" : "Black";
        }
    } else {
        j["legal"] = false;
        const char* reason = "unknown";
        if (r == ERR_OCCUPIED) reason = "occupied";
        else if (r == ERR_SUICIDE) reason = "suicide";
        else if (r == ERR_SUPERKO) reason = "superko";
        else if (r == ERR_FIRST_MOVE_CENTER) reason = "first_move_center";
        else if (r == ERR_NOT_CORE) reason = "not_core";
        else if (r == ERR_NOT_ADJACENT) reason = "not_adjacent";
        else if (r == ERR_CORE_VACANCY) reason = "core_vacancy";
        j["error"] = reason;
    }
    return j;
}

// ── 入口 ──────────────────────────────────────────────────

int main()
{
    httplib::Server svr;

    // ── OPTIONS (CORS preflight) ──
    svr.Options("/api/(.*)", [](const httplib::Request&, httplib::Response& res) {
        addCors(res);
        res.status = 204;
    });

    // ── POST /api/new_game ──
    svr.Post("/api/new_game", [](const httplib::Request& req, httplib::Response& res) {
        addCors(res);
        try {
            auto body = json::parse(req.body);
            int board_size = body.value("board_size", 5);
            if (board_size < 3 || board_size > MAX_BOARD_SIZE) {
                res.status = 400;
                res.set_content(R"({"error":"invalid board_size"})", "application/json");
                return;
            }
            if (g_gs) XinQi_Destroy(g_gs);
            g_gs = XinQi_Create((int8_t)board_size);
            g_terminated = false;
            g_winner = "None";
            g_moves.clear();
            if (!g_gs) {
                res.status = 500;
                res.set_content(R"({"error":"create failed"})", "application/json");
                return;
            }
            json j;
            j["game_id"] = 1;
            j["current_player"] = "Black";
            j["board_size"] = board_size;
            res.set_content(j.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
        }
    });

    // ── POST /api/play ──
    svr.Post("/api/play", [](const httplib::Request& req, httplib::Response& res) {
        addCors(res);
        if (!g_gs) {
            res.status = 400;
            res.set_content(R"({"error":"no game"})", "application/json");
            return;
        }
        if (g_terminated) {
            res.set_content(R"({"legal":false,"error":"game over"})", "application/json");
            return;
        }
        try {
            auto body = json::parse(req.body);
            int8_t x = (int8_t)(int)body["x"];
            int8_t y = (int8_t)(int)body["y"];
            int8_t z = (int8_t)(int)body["z"];
            int8_t played = g_gs->current; // 保存当前玩家（获胜者）

            int8_t r = XinQi_Place(g_gs, x, y, z);
            if (r >= 1) {
                g_terminated = true;
                g_winner = played == COLOR_BLACK ? "Black" : "White";
            } else if (r == RESULT_OK && !XinQi_HasAnyLegalMove(g_gs)) {
                g_terminated = true;
                g_winner = g_gs->current == COLOR_BLACK ? "Black" : "White";
            }
            if (r >= RESULT_OK) {
                json m; m["x"] = x; m["y"] = y; m["z"] = z; m["is_move"] = false; m["result_code"] = r;
                g_moves.push_back(m);
            }
            json j = resultToJson(r, g_gs, x, y, z);
            res.set_content(j.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
        }
    });

    // ── POST /api/move_stone ──
    svr.Post("/api/move_stone", [](const httplib::Request& req, httplib::Response& res) {
        addCors(res);
        if (!g_gs) {
            res.status = 400;
            res.set_content(R"({"error":"no game"})", "application/json");
            return;
        }
        if (g_terminated) {
            res.set_content(R"({"legal":false,"error":"game over"})", "application/json");
            return;
        }
        try {
            auto body = json::parse(req.body);
            int8_t fx = (int8_t)(int)body["x"];
            int8_t fy = (int8_t)(int)body["y"];
            int8_t fz = (int8_t)(int)body["z"];
            int8_t tx = (int8_t)(int)body["target_x"];
            int8_t ty = (int8_t)(int)body["target_y"];
            int8_t tz = (int8_t)(int)body["target_z"];
            int8_t played = g_gs->current;

            int8_t r = XinQi_Shift(g_gs, fx, fy, fz, tx, ty, tz);
            if (r >= 1) {
                g_terminated = true;
                g_winner = played == COLOR_BLACK ? "Black" : "White";
            } else if (r == RESULT_OK && !XinQi_HasAnyLegalMove(g_gs)) {
                g_terminated = true;
                g_winner = g_gs->current == COLOR_BLACK ? "Black" : "White";
            }
            if (r >= RESULT_OK) {
                json m; m["x"] = fx; m["y"] = fy; m["z"] = fz;
                m["target_x"] = tx; m["target_y"] = ty; m["target_z"] = tz;
                m["is_move"] = true; m["result_code"] = r;
                g_moves.push_back(m);
            }
            json j = resultToJson(r, g_gs, tx, ty, tz);
            if (r >= RESULT_OK) {
                j["new_vacancy"] = {{"x", fx}, {"y", fy}, {"z", fz}};
            }
            res.set_content(j.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
        }
    });

    // ── GET /api/state ──
    svr.Get("/api/state", [](const httplib::Request&, httplib::Response& res) {
        addCors(res);
        if (!g_gs) {
            res.status = 400;
            res.set_content(R"({"error":"no game"})", "application/json");
            return;
        }
        res.set_content(stateToJson(g_gs).dump(), "application/json");
    });

    // ── GET /api/serialize ──
    svr.Get("/api/serialize", [](const httplib::Request&, httplib::Response& res) {
        addCors(res);
        if (!g_gs) {
            res.status = 400;
            res.set_content(R"({"error":"no game"})", "application/json");
            return;
        }
        int total = g_gs->size * g_gs->size * g_gs->size;
        const int8_t* board = XinQi_BoardData(g_gs);
        std::string hex = bytes_to_hex((const uint8_t*)board, total);
        json j;
        j["state"] = hex;
        res.set_content(j.dump(), "application/json");
    });

    // ── POST /api/record/save ──
    svr.Post("/api/record/save", [](const httplib::Request&, httplib::Response& res) {
        addCors(res);
        if (!g_gs) {
            res.status = 400;
            res.set_content(R"({"error":"no game"})", "application/json");
            return;
        }
        fs::create_directories("records");

        int total = g_gs->size * g_gs->size * g_gs->size;
        const int8_t* board = XinQi_BoardData(g_gs);
        std::string hex = bytes_to_hex((const uint8_t*)board, total);

        json record;
        record["board_size"] = g_gs->size;
        record["state_hex"] = hex;
        record["move_count"] = g_gs->moveCount;
        record["moves"] = g_moves;
        record["winner"] = g_terminated ? g_winner : "None";

        // 时间戳文件名
        auto t = std::time(nullptr);
        char buf[64];
        struct tm tm_buf;
        localtime_s(&tm_buf, &t);
        std::strftime(buf, sizeof(buf), "%Y%m%d_%H%M%S", &tm_buf);
        std::string filename = std::string("records/xinqi_") + buf + ".json";

        std::ofstream f(filename);
        f << record.dump(2);
        f.close();

        json resp;
        resp["saved"] = filename;
        res.set_content(resp.dump(), "application/json");
    });

    // ── GET /api/record/list ──
    svr.Get("/api/record/list", [](const httplib::Request&, httplib::Response& res) {
        addCors(res);
        fs::create_directories("records");
        json files = json::array();
        for (auto& entry : fs::directory_iterator("records")) {
            if (entry.path().extension() == ".json") {
                files.push_back(entry.path().filename().string());
            }
        }
        json j;
        j["files"] = files;
        res.set_content(j.dump(), "application/json");
    });

    // ── GET /api/record/get ──
    svr.Get(R"(/api/record/get)", [](const httplib::Request& req, httplib::Response& res) {
        addCors(res);
        auto file = req.get_param_value("file");
        if (file.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"missing file"})", "application/json");
            return;
        }
        std::string path = "records/" + file;
        std::ifstream f(path);
        if (!f.is_open()) {
            res.status = 404;
            res.set_content(R"({"error":"not found"})", "application/json");
            return;
        }
        std::string content((std::istreambuf_iterator<char>(f)), {});
        res.set_content(content, "application/json");
    });

    // ── POST /api/record/replay ──
    svr.Post(R"(/api/record/replay)", [](const httplib::Request& req, httplib::Response& res) {
        addCors(res);
        try {
            auto body = json::parse(req.body);
            int N = body["board_size"];
            if (N < 3 || N > MAX_BOARD_SIZE) {
                res.status = 400;
                res.set_content(R"({"error":"invalid board_size"})", "application/json");
                return;
            }

            json steps = json::array();

            // 保存当前游戏状态，创建新棋盘回放
            GameState* replay = XinQi_Create((int8_t)N);
            if (!replay) {
                res.status = 500;
                res.set_content(R"({"error":"create failed"})", "application/json");
                return;
            }

            // 第一步：空棋盘
            {
                json s;
                s["board"] = boardToJson(replay);
                s["current_player"] = "Black";
                s["move_count"] = 0;
                s["inner_core_count"]["Black"] = 0;
                s["inner_core_count"]["White"] = 0;
                s["vacancy_indices"] = json::array();
                steps.push_back(s);
            }

            // 如果提供了走法列表，逐手回放
            bool broken = false;
            if (body.contains("moves") && body["moves"].is_array()) {
                for (auto& m : body["moves"]) {
                    int8_t x = (int8_t)(int)m["x"];
                    int8_t y = (int8_t)(int)m["y"];
                    int8_t z = (int8_t)(int)m["z"];
                    bool isMove = m.value("is_move", false);

                    int8_t r;
                    if (isMove && m.contains("target_x")) {
                        int8_t tx = (int8_t)(int)m["target_x"];
                        int8_t ty = (int8_t)(int)m["target_y"];
                        int8_t tz = (int8_t)(int)m["target_z"];
                        r = XinQi_Shift(replay, x, y, z, tx, ty, tz);
                    } else {
                        r = XinQi_Place(replay, x, y, z);
                    }

                    if (r < 0 && r != WIN_CLEAR_BOARD && r != WIN_CORE_INVASION && r != WIN_NO_LEGAL_MOVE) {
                        broken = true;
                        break;
                    }

                    // 计算内芯数
                    const int8_t* rboard = XinQi_BoardData(replay);
                    int blk = 0, wht = 0;
                    int rtotal = N * N * N;
                    for (int i = 0; i < rtotal; ++i) {
                        if (rboard[i] != CELL_BLACK && rboard[i] != CELL_WHITE) continue;
                        int8_t rx = (int8_t)(i / (N * N));
                        int8_t ry = (int8_t)((i / N) % N);
                        int8_t rz = (int8_t)(i % N);
                        if (rboard[i] == CELL_BLACK && XinQi_IsCore(replay, rx, ry, rz)) blk++;
                        if (rboard[i] == CELL_WHITE && XinQi_IsCore(replay, rx, ry, rz)) wht++;
                    }

                    json s;
                    s["board"] = boardToJson(replay);
                    s["current_player"] = replay->current == COLOR_BLACK ? "Black" : "White";
                    s["move_count"] = replay->moveCount;
                    s["inner_core_count"]["Black"] = blk;
                    s["inner_core_count"]["White"] = wht;

                    // 内芯空位
                    json vac = json::array();
                    for (int i = 0; i < rtotal; ++i) {
                        if (rboard[i] == CELL_BLACK_VACANCY || rboard[i] == CELL_WHITE_VACANCY) {
                            vac.push_back(i);
                        }
                    }
                    s["vacancy_indices"] = vac;
                    s["last_move"] = {{"x", x}, {"y", y}, {"z", z}};
                    s["last_move_is_move"] = isMove;
                    s["result_code"] = r;
                    steps.push_back(s);
                }
            }

            XinQi_Destroy(replay);

            json j;
            j["total"] = (int)steps.size();
            j["steps"] = steps;
            j["broken"] = broken;
            j["winner"] = "None";
            res.set_content(j.dump(), "application/json");

        } catch (const std::exception& e) {
            res.status = 500;
            json err; err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── POST /api/ai/move ──
    svr.Post("/api/ai/move", [](const httplib::Request& req, httplib::Response& res) {
        addCors(res);
        if (!g_gs) {
            res.status = 400;
            res.set_content(R"({"error":"no game"})", "application/json");
            return;
        }
        try {
            auto body = json::parse(req.body);
            int sims = body.value("simulations", 2000);

            AIConfig cfg;
            cfg.simulations = sims;
            cfg.explorationC = 1.414;

            AIMove move = AI_Think(g_gs, &cfg);

            json j;
            if (move.type < 0) {
                j["error"] = "no_move";
            } else {
                j["x"] = move.x;
                j["y"] = move.y;
                j["z"] = move.z;
                j["type"] = move.type;
                if (move.type == MOVE_SHIFT) {
                    j["target_x"] = move.tx;
                    j["target_y"] = move.ty;
                    j["target_z"] = move.tz;
                }
            }
            res.set_content(j.dump(), "application/json");
        } catch (const std::exception& e) {
            res.status = 500;
            json err; err["error"] = e.what();
            res.set_content(err.dump(), "application/json");
        }
    });

    // ── GET /api/ai/status ──
    svr.Get("/api/ai/status", [](const httplib::Request&, httplib::Response& res) {
        addCors(res);
        json j;
        j["engine"] = "xinqi_mcts";
        j["version"] = 1;
        j["max_simulations"] = 10000;
        j["default_simulations"] = 2000;
        res.set_content(j.dump(), "application/json");
    });

    // ── 静态文件：前端 dist/ ──
    // 从 exe 所在目录查找 dist/
    std::string distPath;
    auto exeDir = fs::path(__argv[0]).parent_path();
    std::vector<std::string> candidates = {
        (exeDir / "dist").string(),
        (fs::current_path() / "dist").string(),
        (exeDir / ".." / ".." / ".." / "dist").string(),
    };
    for (auto& p : candidates) {
        if (fs::exists(p)) { distPath = p; break; }
    }
    if (!distPath.empty()) {
        svr.set_mount_point("/", distPath);
        printf("Frontend: %s\n", distPath.c_str());
    } else {
        printf("Warning: dist/ not found at %s\n", distPath.c_str());
        // 根路径返回简单提示
        svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("<h1>XinQi Server Running</h1>"
                            "<p>Frontend not built. Run <code>cd xinqi-frontend && npm run build</code></p>"
                            "<p>API: <a href='/api/state'>/api/state</a></p>", "text/html");
        });
    }

    // ── 前端 SPA fallback：所有非 /api/ 路由返回 index.html ──
    svr.Get(R"(/(.*))", [](const httplib::Request& req, httplib::Response& res) {
        if (req.path.find("/api/") == 0) {
            res.status = 404;
            res.set_content("{\"error\":\"not found\"}", "application/json");
            return;
        }
        // 如果是静态文件（有扩展名），让 httplib 走默认 404
        if (req.path.find_last_of('.') != std::string::npos) {
            res.status = 404;
            return;
        }
        // SPA fallback
        std::string index = (fs::current_path() / "dist" / "index.html").string();
        std::ifstream f(index);
        if (f.is_open()) {
            std::string content((std::istreambuf_iterator<char>(f)), {});
            res.set_content(content, "text/html");
        } else {
            res.set_content("Frontend not built yet.", "text/plain");
        }
    });

    // ── 启动 ──
    int port = 8090;
    printf("XinQiServer listening on http://localhost:%d\n", port);
    svr.listen("127.0.0.1", port);

    if (g_gs) XinQi_Destroy(g_gs);
    return 0;
}
