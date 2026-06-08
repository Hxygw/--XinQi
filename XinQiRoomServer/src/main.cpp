/**
 * XinQi Room Server — 多人对战服务端
 *
 * 编译: MSBuild XinQiRoomServer.vcxproj /p:Configuration=Release /p:Platform=x64
 * 运行: XinQiRoomServer\x64\Release\XinQiRoomServer.exe
 * 暴露: ngrok http 8090
 *
 * 依赖: XinQiCore.lib, httplib.h, json.hpp
 * 无 AI、无棋谱、纯房间对战
 */

#include <XinQiCore.h>
#include <httplib.h>
#include <json.hpp>
#include <string>
#include <map>
#include <mutex>
#include <random>
#include <thread>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
namespace fs = std::filesystem;

using json = nlohmann::json;

// ══════════════════════════════════════════════════════════════
// 房间管理
// ══════════════════════════════════════════════════════════════

struct Room {
    GameState* gs = nullptr;
    bool terminated = false;
    std::string winner = "None";
    std::vector<json> moves;
    std::string hostId;       // 房主 playerId
    std::string blackId;      // 黑方 playerId (房主)
    std::string whiteId;      // 白方 playerId (加入者)
    std::string hostAddr;     // 房主 IP 地址
    std::string guestAddr;    // 客人 IP 地址
    bool started = false;     // 是否已开局
    int boardSize = 5;
    std::chrono::steady_clock::time_point createdAt;
    std::time_t createdAtTime = 0;  // 可读时间
    bool hasGuest = false;    // 是否有客人加入
};

static std::map<std::string, Room> g_rooms;
static std::mutex g_roomsMutex;

// ── 房间信息打印 ──

static std::string timeStr(std::time_t t) {
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", std::localtime(&t));
    return buf;
}

static std::string roomStatus(const Room& room) {
    if (room.terminated) return "终局";
    if (room.started) return "对局中";
    if (room.hasGuest) return "等待开局";
    return "等待加入";
}

static void printRooms() {
    printf("\n=== 房间列表 (%zu 个) ===\n", g_rooms.size());
    for (auto& [code, room] : g_rooms) {
        printf("  %s  [%s]  %s  房主: %s",
            code.c_str(), timeStr(room.createdAtTime).c_str(),
            roomStatus(room).c_str(), room.hostAddr.c_str());
        if (room.hasGuest) printf("  客人: %s", room.guestAddr.c_str());
        if (room.terminated) printf("  %s胜", room.winner.c_str());
        printf("\n");
    }
    printf("========================\n");
}

// ── 工具函数 ──

static std::string randomCode() {
    static const char chars[] = "0123456789";
    static std::mt19937 rng((uint32_t)std::time(nullptr));
    std::string code;
    for (int i = 0; i < 4; ++i) code += chars[rng() % 10];
    return code;
}

static std::string randomId() {
    static std::mt19937 rng((uint32_t)std::time(nullptr));
    std::string id = "p_";
    for (int i = 0; i < 8; ++i) {
        id += "0123456789abcdef"[rng() % 16];
    }
    return id;
}

static json boardToJson(const GameState* gs) {
    int N = gs->size;
    int total = N * N * N;
    const int8_t* board = XinQi_BoardData(gs);
    json arr = json::array();
    for (int i = 0; i < total; ++i) arr.push_back((int)board[i]);
    return arr;
}

static json stateToJson(const Room& room) {
    json j;
    j["board_size"] = room.gs->size;
    j["board"] = boardToJson(room.gs);
    j["current_player"] = room.gs->current == COLOR_BLACK ? "Black" : "White";
    j["move_count"] = room.gs->moveCount;
    j["terminal"] = room.terminated;
    j["started"] = room.started;
    j["has_guest"] = room.hasGuest;
    if (room.terminated) j["winner"] = room.winner;

    const int8_t* board = XinQi_BoardData(room.gs);
    int N = room.gs->size;
    int total = N * N * N;
    int blackCores = 0, whiteCores = 0;

    // 内芯计数
    for (int i = 0; i < total; ++i) {
        if (board[i] != CELL_BLACK && board[i] != CELL_WHITE) continue;
        int8_t x = (int8_t)(i / (N * N));
        int8_t y = (int8_t)((i / N) % N);
        int8_t z = (int8_t)(i % N);
        if (board[i] == CELL_BLACK && XinQi_IsCore(room.gs, x, y, z)) blackCores++;
        if (board[i] == CELL_WHITE && XinQi_IsCore(room.gs, x, y, z)) whiteCores++;
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

// ── 清理过期房间线程 ──
static void cleanupThread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::minutes(30));
        std::lock_guard<std::mutex> lock(g_roomsMutex);
        auto now = std::chrono::steady_clock::now();
        for (auto it = g_rooms.begin(); it != g_rooms.end(); ) {
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - it->second.createdAt).count();
            if (age >= 24) {
                if (it->second.gs) XinQi_Destroy(it->second.gs);
                it = g_rooms.erase(it);
            } else {
                ++it;
            }
        }
    }
}

// ══════════════════════════════════════════════════════════════
// 入口
// ══════════════════════════════════════════════════════════════

int main() {
    httplib::Server svr;

    // ── 全局 CORS ──
    svr.set_default_headers({
        {"Access-Control-Allow-Origin", "*"},
        {"Access-Control-Allow-Methods", "GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", "Content-Type"},
    });

    // ── CORS 预检 ──
    svr.Options(R"(/.*)", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content("", "text/plain");
    });

    // ── POST /api/room/create ──
    svr.Post("/api/room/create", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            int boardSize = body.value("board_size", 5);
            if (boardSize < 3 || boardSize > MAX_BOARD_SIZE) {
                res.status = 400;
                res.set_content(R"({"error":"invalid board_size"})", "application/json");
                return;
            }

            std::lock_guard<std::mutex> lock(g_roomsMutex);

            std::string code;
            do { code = randomCode(); } while (g_rooms.count(code));

            std::string hostId = randomId();
            Room room;
            room.gs = nullptr;  // 开局时才创建
            room.hostId = hostId;
            room.blackId = hostId;
            room.boardSize = boardSize;
            room.createdAt = std::chrono::steady_clock::now();
            room.createdAtTime = std::time(nullptr);
            room.hostAddr = req.remote_addr;
            g_rooms[code] = std::move(room);
            printRooms();

            json j;
            j["room_code"] = code;
            j["player_id"] = hostId;
            j["role"] = "Black";
            j["board_size"] = boardSize;
            res.set_content(j.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
        }
    });

    // ── POST /api/room/join ──
    svr.Post("/api/room/join", [](const httplib::Request& req, httplib::Response& res) {
        try {
            auto body = json::parse(req.body);
            std::string code = body["room_code"];

            std::lock_guard<std::mutex> lock(g_roomsMutex);

            auto it = g_rooms.find(code);
            if (it == g_rooms.end()) {
                res.status = 404;
                res.set_content(R"({"error":"room not found"})", "application/json");
                return;
            }
            Room& room = it->second;
            if (room.hasGuest) {
                res.status = 400;
                res.set_content(R"({"error":"room is full"})", "application/json");
                return;
            }
            if (room.started) {
                res.status = 400;
                res.set_content(R"({"error":"game already started"})", "application/json");
                return;
            }

            std::string playerId = randomId();
            room.whiteId = playerId;
            room.hasGuest = true;
            room.guestAddr = req.remote_addr;
            printRooms();

            json j;
            j["player_id"] = playerId;
            j["role"] = "White";
            j["board_size"] = room.boardSize;
            j["host_id"] = room.hostId;
            res.set_content(j.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid json"})", "application/json");
        }
    });

    // ── POST /api/room/:code/set_size ──
    svr.Post(R"(/api/room/(\d+)/set_size)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string code = req.matches[1];
            auto body = json::parse(req.body);
            int boardSize = body.value("board_size", 5);
            std::lock_guard<std::mutex> lock(g_roomsMutex);
            auto it = g_rooms.find(code);
            if (it == g_rooms.end()) { res.status = 404; res.set_content(R"({"error":"not found"})", "application/json"); return; }
            Room& room = it->second;
            if (room.started) { res.status = 400; res.set_content(R"({"error":"already started"})", "application/json"); return; }
            if (boardSize < 3 || boardSize > MAX_BOARD_SIZE) { res.status = 400; res.set_content(R"({"error":"invalid size"})", "application/json"); return; }
            room.boardSize = boardSize;
            res.set_content(R"({"ok":true})", "application/json");
        } catch (...) { res.status = 400; res.set_content(R"({"error":"invalid"})", "application/json"); }
    });

    // ── POST /api/room/:code/start ──
    svr.Post(R"(/api/room/(\d+)/start)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string code = req.matches[1];
            auto body = json::parse(req.body);
            std::string playerId = body["player_id"];

            std::lock_guard<std::mutex> lock(g_roomsMutex);
            auto it = g_rooms.find(code);
            if (it == g_rooms.end()) {
                res.status = 404;
                res.set_content(R"({"error":"room not found"})", "application/json");
                return;
            }
            Room& room = it->second;
            if (room.hostId != playerId) {
                res.status = 403;
                res.set_content(R"({"error":"only host can start"})", "application/json");
                return;
            }
            if (room.started) {
                res.status = 400;
                res.set_content(R"({"error":"already started"})", "application/json");
                return;
            }
            if (!room.hasGuest) {
                res.status = 400;
                res.set_content(R"({"error":"waiting for opponent"})", "application/json");
                return;
            }
            // 允许房主在开局前修改棋盘大小
            room.boardSize = body.value("board_size", room.boardSize);
            // 创建游戏状态
            room.gs = XinQi_Create((int8_t)room.boardSize);
            if (!room.gs) {
                res.status = 500;
                res.set_content(R"({"error":"create failed"})", "application/json");
                return;
            }
            room.started = true;
            printRooms();
            res.set_content(R"({"ok":true})", "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid"})", "application/json");
        }
    });

    // ── POST /api/room/:code/play ──
    svr.Post(R"(/api/room/(\d+)/play)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string code = req.matches[1];
            auto body = json::parse(req.body);
            std::string playerId = body["player_id"];
            int8_t x = (int8_t)(int)body["x"];
            int8_t y = (int8_t)(int)body["y"];
            int8_t z = (int8_t)(int)body["z"];

            std::lock_guard<std::mutex> lock(g_roomsMutex);
            auto it = g_rooms.find(code);
            if (it == g_rooms.end()) {
                res.status = 404;
                res.set_content(R"({"error":"room not found"})", "application/json");
                return;
            }
            Room& room = it->second;
            if (!room.started) {
                res.status = 400;
                res.set_content(R"({"error":"game not started"})", "application/json");
                return;
            }
            if (!room.gs) {
                res.status = 500;
                res.set_content(R"({"error":"game state is null"})", "application/json");
                return;
            }
            if (room.terminated) {
                res.set_content(R"({"legal":false,"error":"game over"})", "application/json");
                return;
            }

            // 校验回合
            int8_t current = room.gs->current;
            std::string expectedId = (current == COLOR_BLACK) ? room.blackId : room.whiteId;
            if (playerId != expectedId) {
                res.status = 403;
                res.set_content(R"({"legal":false,"error":"not your turn"})", "application/json");
                return;
            }

            int8_t played = room.gs->current;
            int8_t r = XinQi_Place(room.gs, x, y, z);
            if (r >= 1) {
                room.terminated = true;
                room.winner = played == COLOR_BLACK ? "Black" : "White";
                printRooms();
            }
            if (r >= RESULT_OK) {
                json m; m["x"] = x; m["y"] = y; m["z"] = z; m["is_move"] = false;
                room.moves.push_back(m);
            }

            // 构建响应（复用 resultToJson 逻辑）
            json j;
            if (r >= 1) {
                // 胜利
                j["legal"] = true; j["terminal"] = true;
                j["result_code"] = r;
                j["winner"] = (room.gs->current == COLOR_BLACK) ? "White" : "Black";
                j["next_player"] = (room.gs->current == COLOR_BLACK) ? "Black" : "White";
                j["captured"] = json::array();
                j["captured_count"] = room.gs->lastCaptureCount;
            } else if (r == RESULT_OK) {
                j["legal"] = true; j["result_code"] = 0;
                j["terminal"] = !XinQi_HasAnyLegalMove(room.gs);
                j["next_player"] = (room.gs->current == COLOR_BLACK) ? "Black" : "White";
                j["captured"] = json::array();
                j["captured_count"] = room.gs->lastCaptureCount;
                if (j["terminal"].get<bool>()) {
                    j["winner"] = (room.gs->current == COLOR_BLACK) ? "White" : "Black";
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
            res.set_content(j.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid"})", "application/json");
        }
    });

    // ── POST /api/room/:code/move_stone ──
    svr.Post(R"(/api/room/(\d+)/move_stone)", [](const httplib::Request& req, httplib::Response& res) {
        try {
            std::string code = req.matches[1];
            auto body = json::parse(req.body);
            std::string playerId = body["player_id"];
            int8_t fx = (int8_t)(int)body["x"];
            int8_t fy = (int8_t)(int)body["y"];
            int8_t fz = (int8_t)(int)body["z"];
            int8_t tx = (int8_t)(int)body["target_x"];
            int8_t ty = (int8_t)(int)body["target_y"];
            int8_t tz = (int8_t)(int)body["target_z"];

            std::lock_guard<std::mutex> lock(g_roomsMutex);
            auto it = g_rooms.find(code);
            if (it == g_rooms.end()) { res.status = 404; res.set_content(R"({"error":"not found"})", "application/json"); return; }
            Room& room = it->second;
            if (!room.started) { res.status = 400; res.set_content(R"({"error":"not started"})", "application/json"); return; }
            if (!room.gs) { res.status = 500; res.set_content(R"({"error":"null gs"})", "application/json"); return; }
            if (room.terminated) { res.set_content(R"({"legal":false,"error":"game over"})", "application/json"); return; }

            int8_t current = room.gs->current;
            std::string expectedId = (current == COLOR_BLACK) ? room.blackId : room.whiteId;
            if (playerId != expectedId) { res.status = 403; res.set_content(R"({"legal":false,"error":"not your turn"})", "application/json"); return; }

            int8_t played = room.gs->current;
            int8_t r = XinQi_Shift(room.gs, fx, fy, fz, tx, ty, tz);
            if (r >= 1) { room.terminated = true; room.winner = played == COLOR_BLACK ? "Black" : "White"; printRooms(); }
            if (r >= RESULT_OK) {
                json m; m["x"] = fx; m["y"] = fy; m["z"] = fz;
                m["target_x"] = tx; m["target_y"] = ty; m["target_z"] = tz; m["is_move"] = true;
                room.moves.push_back(m);
            }
            json j;
            if (r >= 1) {
                j["legal"] = true; j["terminal"] = true; j["result_code"] = r;
                j["winner"] = (room.gs->current == COLOR_BLACK) ? "White" : "Black";
                j["next_player"] = (room.gs->current == COLOR_BLACK) ? "Black" : "White";
                j["captured"] = json::array(); j["captured_count"] = room.gs->lastCaptureCount;
                j["new_vacancy"] = {{"x", fx}, {"y", fy}, {"z", fz}};
            } else if (r == RESULT_OK) {
                j["legal"] = true; j["result_code"] = 0;
                j["terminal"] = !XinQi_HasAnyLegalMove(room.gs);
                j["next_player"] = (room.gs->current == COLOR_BLACK) ? "Black" : "White";
                j["captured"] = json::array(); j["captured_count"] = room.gs->lastCaptureCount;
                j["new_vacancy"] = {{"x", fx}, {"y", fy}, {"z", fz}};
                if (j["terminal"].get<bool>()) j["winner"] = (room.gs->current == COLOR_BLACK) ? "White" : "Black";
            } else {
                j["legal"] = false; j["error"] = "illegal";
            }
            res.set_content(j.dump(), "application/json");
        } catch (...) {
            res.status = 400;
            res.set_content(R"({"error":"invalid"})", "application/json");
        }
    });

    // ── GET /api/room/:code/state ──
    svr.Get(R"(/api/room/(\d+)/state)", [](const httplib::Request& req, httplib::Response& res) {
        std::string code = req.matches[1];
        std::lock_guard<std::mutex> lock(g_roomsMutex);
        auto it = g_rooms.find(code);
        if (it == g_rooms.end()) {
            res.status = 404;
            res.set_content(R"({"error":"room not found"})", "application/json");
            return;
        }
        Room& room = it->second;
        if (!room.gs) {
            json j; j["started"] = false; j["board_size"] = room.boardSize;
            res.set_content(j.dump(), "application/json");
            return;
        }
        res.set_content(stateToJson(it->second).dump(), "application/json");
    });

    // ── GET /api/room/:code/info ──
    svr.Get(R"(/api/room/(\d+)/info)", [](const httplib::Request& req, httplib::Response& res) {
        std::string code = req.matches[1];
        std::lock_guard<std::mutex> lock(g_roomsMutex);
        auto it = g_rooms.find(code);
        if (it == g_rooms.end()) {
            res.status = 404;
            res.set_content(R"({"error":"not found"})", "application/json");
            return;
        }
        Room& room = it->second;
        json j;
        j["board_size"] = room.boardSize;
        j["has_guest"] = room.hasGuest;
        j["guest_id"] = room.hasGuest ? room.whiteId : nullptr;
        j["host_id"] = room.hostId;
        j["started"] = room.started;
        j["terminated"] = room.terminated;
        if (room.terminated) j["winner"] = room.winner;
        res.set_content(j.dump(), "application/json");
    });

    // ── 静态文件 + SPA fallback ──
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
        printf("Frontend: %s\n", distPath.c_str());
        svr.Get(R"(/(.*))", [distPath](const httplib::Request& req, httplib::Response& res) {
            if (req.path.find("/api/") == 0) {
                res.status = 404;
                res.set_content("{\"error\":\"not found\"}", "application/json");
                return;
            }
            std::string filePath = (fs::path(distPath) / req.path.substr(1)).string();
            if (req.path == "/") filePath = (fs::path(distPath) / "index.html").string();
            std::ifstream f(filePath, std::ios::binary);
            if (f.is_open()) {
                std::string content((std::istreambuf_iterator<char>(f)), {});
                auto ext = fs::path(filePath).extension().string();
                if (ext == ".html") res.set_content(content, "text/html; charset=utf-8");
                else if (ext == ".css") res.set_content(content, "text/css; charset=utf-8");
                else if (ext == ".js") res.set_content(content, "application/javascript; charset=utf-8");
                else if (ext == ".svg") res.set_content(content, "image/svg+xml");
                else res.set_content(content, "application/octet-stream");
            } else {
                std::string indexPath = (fs::path(distPath) / "index.html").string();
                std::ifstream fi(indexPath);
                if (fi.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(fi)), {});
                    res.set_content(content, "text/html; charset=utf-8");
                } else {
                    res.status = 404;
                    res.set_content("Not found", "text/plain");
                }
            }
        });
    } else {
        printf("Warning: dist/ not found\n");
        svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content("<h1>XinQi Room Server</h1><p>Frontend not built.</p>", "text/html");
        });
    }

    // ── 启动清理线程 ──
    std::thread cleaner(cleanupThread);
    cleaner.detach();

    int port = 8090;
    printf("XinQi Room Server listening on http://0.0.0.0:%d\n", port);
    printf("Share via: ngrok http %d\n", port);
    fflush(stdout);

    try {
        svr.listen("0.0.0.0", port);
    } catch (const std::exception& e) {
        printf("FATAL: %s\n", e.what());
        return 1;
    }

    // 清理
    for (auto& [_, room] : g_rooms) {
        if (room.gs) XinQi_Destroy(room.gs);
    }
    return 0;
}
// force recompile  
