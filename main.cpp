
#include <iostream>
#include <string>
#include <cmath> // sqrt, pow, floor用
#include <unordered_map>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>
#ifdef _WIN32
#include <windows.h>
#endif

// --- 前回設計したマスターデータ用の箱 ---
struct PokemonBase {
    std::string name;
    int attack;
    int defense;
    int hp;
    std::string type1;
    std::string type2;
};

struct FastMove {
    std::string name;
    std::string type;
    int power;
    double time;
    double damage_window;
    double eps;
    double dps;
};

struct ChargeMove {
    std::string name;
    std::string type;
    int power;
    double time;
    double damage_window;
    double dps;
    int gauge;
};

struct MegaData {
    std::string mega_name;
    std::string base_name;
    std::string exclusive_move;
};

// マスターデータを格納するグローバル変数（メイン関数等でCSVから読み込んでおく）
std::unordered_map<std::string, PokemonBase> pokemon_db;
std::map<double, double> cpm_db;
std::unordered_map<std::string, FastMove> fast_move_db;
std::unordered_map<std::string, ChargeMove> charge_move_db;
std::unordered_map<std::string, MegaData> mega_db;

// --- ユーザーのポケモン ---
struct UserPokemon {
    int id;
    std::string name;
    
    double pl;
    int aiv, biv, hiv;
    
    std::string normalatk, specialatk, thaadatk;
    
    bool is_shadow = false;
    bool is_purified = false;
    bool is_lucky = false;
    bool has_buddy_boost = false;
    int mega_level = 0; // 0:なし, 1~3:メガシンカ状態(未実装の場合は0)
    
    std::string active_mega = "";
    std::string active_mega_move = "";

    bool is_protected = false;

    int cp = 0;

    // CPを再計算して更新する関数
    void update_cp() {
        // 1. 種族値をデータベースから検索
        std::string search_name = active_mega.empty() ? name : active_mega;
        auto poke_it = pokemon_db.find(search_name);
        if (poke_it == pokemon_db.end()) {
            cp = 0; // エラー時の安全処理
            return;
        }
        const PokemonBase& base = poke_it->second;

        // 2. 実質的なPL（相棒ブーストの処理）
        double effective_pl = pl;
        if (has_buddy_boost) effective_pl += 1.0;
        if (mega_level == 4 && !active_mega.empty()) effective_pl += 2.0; // メガシンカ中でメガレベル4なら+2
        if (effective_pl > 55.0) effective_pl = 55.0; // 上限を51.0から55.0に引き上げ

        // 3. CPMをデータベースから検索
        auto cpm_it = cpm_db.find(effective_pl);
        if (cpm_it == cpm_db.end()) {
            cp = 0; // エラー時の安全処理
            return;
        }
        double cpm = cpm_it->second;

        // 4. 数式に当てはめて計算
        double calc = (base.attack + aiv) 
                    * std::sqrt(base.defense + biv) 
                    * std::sqrt(base.hp + hiv) 
                    * std::pow(cpm, 2) 
                    / 10.0;

        // 5. 切り捨てて整数化、最低10を保証
        cp = static_cast<int>(std::floor(calc));
        if (cp < 10) {
            cp = 10;
        }
    }

    std::vector<std::pair<std::string, double>> calc_all_effective_dps() const {
        if (normalatk.empty()) return {};

        std::string search_name = active_mega.empty() ? name : active_mega;
        auto poke_it = pokemon_db.find(search_name);
        if (poke_it == pokemon_db.end()) return {};
        const auto& base = poke_it->second;

        double effective_pl = pl;
        if (has_buddy_boost) effective_pl += 1.0;
        if (mega_level == 4 && !active_mega.empty()) effective_pl += 2.0; // メガシンカ中でメガレベル4なら+2
        if (effective_pl > 55.0) effective_pl = 55.0; // 上限を51.0から55.0に引き上げ

        auto cpm_it = cpm_db.find(effective_pl);
        if (cpm_it == cpm_db.end()) return {};
        double cpm = cpm_it->second;

        double a_actual = (base.attack + aiv) * cpm;

        auto fast_it = fast_move_db.find(normalatk);
        if (fast_it == fast_move_db.end()) return {};
        const auto& fast = fast_it->second;

        std::vector<std::pair<std::string, double>> results;
        // specialatk, thaadatk, active_mega_move のうち、空文字でないもの
        std::vector<std::string> target_moves;
        if (!specialatk.empty()) target_moves.push_back(specialatk);
        if (!thaadatk.empty()) target_moves.push_back(thaadatk);
        if (!active_mega_move.empty()) target_moves.push_back(active_mega_move);

        for (const auto& move_name : target_moves) {
            auto charge_it = charge_move_db.find(move_name);
            if (charge_it == charge_move_db.end()) continue;
            const auto& charge = charge_it->second;

            double p_s = charge.power;
            if (move_name == active_mega_move) {
                p_s *= (0.9 + 0.1 * mega_level);
            }

            double e_req = (charge.gauge == 1) ? 100.0 : (charge.gauge == 2) ? 50.0 : 33.0;
            double e_gain = fast.time * fast.eps;
            if (e_gain <= 0) continue; // 回避用

            double n = std::ceil(e_req / e_gain);

            double stab_n = (fast.type == base.type1 || fast.type == base.type2) ? 1.2 : 1.0;
            double stab_s = (charge.type == base.type1 || charge.type == base.type2) ? 1.2 : 1.0;

            double d_cycle = (fast.power * stab_n * n) + (p_s * stab_s);
            double t_cycle = (fast.time * n) + charge.time;

            if (t_cycle <= 0) continue;

            double dps_base = d_cycle / t_cycle;
            double shadow_bonus = is_shadow ? 1.2 : 1.0;
            double effective_dps = dps_base * a_actual * 0.5 * shadow_bonus;

            results.push_back({move_name, effective_dps});
        }

        return results;
    }
};

#include <fstream>

// ユーザーの手持ちポケモン管理用
std::vector<UserPokemon> my_box;
int next_id = 1;

// カンマ区切りの文字列を分割するヘルパー関数
std::vector<std::string> split_csv_line(const std::string& line) {
    std::vector<std::string> result;
    std::stringstream ss(line);
    std::string item;
    while (std::getline(ss, item, ',')) {
        result.push_back(item);
    }
    return result;
}

void load_databases() {
    std::string line;

    // base.csv 読み込み
    std::ifstream ifs_base("base.csv");
    if (ifs_base) {
        std::getline(ifs_base, line); // ヘッダースキップ
        while (std::getline(ifs_base, line)) {
            if (line.empty()) continue;
            auto cols = split_csv_line(line);
            if (cols.size() >= 6) {
                PokemonBase p;
                p.name = cols[0];
                p.attack = std::stoi(cols[1]);
                p.defense = std::stoi(cols[2]);
                p.hp = std::stoi(cols[3]);
                p.type1 = cols[4];
                p.type2 = cols[5] == "n" ? "" : cols[5];
                pokemon_db[p.name] = p;
            }
        }
    } else {
        std::cerr << "Error: base.csv を開けませんでした。" << std::endl;
    }

    // cpm.csv 読み込み
    std::ifstream ifs_cpm("cpm.csv");
    if (ifs_cpm) {
        std::getline(ifs_cpm, line); // ヘッダースキップ
        while (std::getline(ifs_cpm, line)) {
            if (line.empty()) continue;
            auto cols = split_csv_line(line);
            if (cols.size() >= 2) {
                double pl = std::stod(cols[0]);
                double cpm = std::stod(cols[1]);
                cpm_db[pl] = cpm;
            }
        }
    } else {
        std::cerr << "Error: cpm.csv を開けませんでした。" << std::endl;
    }

    // normal.csv 読み込み
    std::ifstream ifs_normal("normal.csv");
    if (ifs_normal) {
        std::getline(ifs_normal, line); // ヘッダースキップ
        while (std::getline(ifs_normal, line)) {
            if (line.empty()) continue;
            auto cols = split_csv_line(line);
            if (cols.size() >= 7) {
                FastMove fm;
                fm.name = cols[0];
                fm.type = cols[1];
                fm.power = std::stoi(cols[2]);
                fm.time = std::stod(cols[3]);
                fm.damage_window = std::stod(cols[4]);
                fm.eps = std::stod(cols[5]);
                fm.dps = std::stod(cols[6]);
                fast_move_db[fm.name] = fm;
            }
        }
    } else {
        std::cerr << "Error: normal.csv を開けませんでした。" << std::endl;
    }

    // special.csv 読み込み
    std::ifstream ifs_special("special.csv");
    if (ifs_special) {
        std::getline(ifs_special, line); // ヘッダースキップ
        while (std::getline(ifs_special, line)) {
            if (line.empty()) continue;
            auto cols = split_csv_line(line);
            if (cols.size() >= 7) {
                ChargeMove cm;
                cm.name = cols[0];
                cm.type = cols[1];
                cm.power = std::stoi(cols[2]);
                cm.time = std::stod(cols[3]);
                cm.damage_window = std::stod(cols[4]);
                cm.dps = std::stod(cols[5]);
                cm.gauge = std::stoi(cols[6]);
                charge_move_db[cm.name] = cm;
            }
        }
    } else {
        std::cerr << "Error: special.csv を開けませんでした。" << std::endl;
    }

    // mega.csv 読み込み
    std::ifstream ifs_mega("mega.csv");
    if (ifs_mega) {
        std::getline(ifs_mega, line); // ヘッダースキップ
        while (std::getline(ifs_mega, line)) {
            if (line.empty()) continue;
            auto cols = split_csv_line(line);
            if (cols.size() >= 3) {
                MegaData md;
                md.mega_name = cols[0];
                md.base_name = cols[1];
                md.exclusive_move = cols[2] == "n" ? "" : cols[2];
                mega_db[md.mega_name] = md;
            }
        }
    } else {
        std::cerr << "Error: mega.csv を開けませんでした。" << std::endl;
    }
}

void save_box() {
    std::ofstream ofs("my_box.csv");
    for (const auto& p : my_box) {
        ofs << p.id << "," << p.name << "," << p.pl << ","
            << p.aiv << "," << p.biv << "," << p.hiv << ","
            << (p.normalatk.empty() ? "none" : p.normalatk) << ","
            << (p.specialatk.empty() ? "none" : p.specialatk) << ","
            << (p.thaadatk.empty() ? "none" : p.thaadatk) << ","
            << p.is_shadow << "," << p.is_purified << "," << p.is_lucky << ","
            << p.has_buddy_boost << "," << p.mega_level << ","
            << (p.active_mega.empty() ? "none" : p.active_mega) << ","
            << (p.active_mega_move.empty() ? "none" : p.active_mega_move) << ","
            << p.is_protected << "\n";
    }
    std::cout << "ボックスを my_box.csv に保存しました（" << my_box.size() << "匹）" << std::endl;
}

void load_box() {
    std::ifstream ifs("my_box.csv");
    if (!ifs) return;
    std::string line;
    my_box.clear();
    int max_id = 0;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        auto cols = split_csv_line(line);
        if (cols.size() >= 16) {
            UserPokemon p;
            p.id = std::stoi(cols[0]);
            p.name = cols[1];
            p.pl = std::stod(cols[2]);
            p.aiv = std::stoi(cols[3]);
            p.biv = std::stoi(cols[4]);
            p.hiv = std::stoi(cols[5]);
            p.normalatk = (cols[6] == "none" ? "" : cols[6]);
            p.specialatk = (cols[7] == "none" ? "" : cols[7]);
            p.thaadatk = (cols[8] == "none" ? "" : cols[8]);
            p.is_shadow = std::stoi(cols[9]);
            p.is_purified = std::stoi(cols[10]);
            p.is_lucky = std::stoi(cols[11]);
            p.has_buddy_boost = std::stoi(cols[12]);
            p.mega_level = std::stoi(cols[13]);
            p.active_mega = (cols[14] == "none" ? "" : cols[14]);
            p.active_mega_move = (cols[15] == "none" ? "" : cols[15]);
            if (cols.size() >= 17) {
                p.is_protected = std::stoi(cols[16]);
            } else {
                p.is_protected = false;
            }
            p.update_cp();
            my_box.push_back(p);
            if (p.id > max_id) max_id = p.id;
        }
    }
    next_id = max_id + 1;
    std::cout << "my_box.csv から " << my_box.size() << "匹ロードしました" << std::endl;
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(65001); // これがないと文字化けする
    SetConsoleCP(65001);
#endif
    load_databases();
    load_box();

    std::string line;
    std::cout << "> ";
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            std::cout << "> ";
            continue;
        }

        std::istringstream iss(line);
        std::string cmd;
        if (!(iss >> cmd)) {
            std::cout << "> ";
            continue;
        }

        if (cmd == "save") {
            save_box();
            std::cout << "> ";
            continue;
        }

        if (cmd == "add") {
            std::string subcmd;
            iss >> subcmd;
            if (subcmd == "pokemon") {
                std::string name;
                double pl;
                int aiv, biv, hiv;
                if (iss >> name >> pl >> aiv >> biv >> hiv) {
                    if (pl < 1.0 || pl > 50.0) {
                        std::cout << "Error: PLは1.0から50.0の間で指定してください。" << std::endl;
                    } else if (aiv < 0 || aiv > 15 || biv < 0 || biv > 15 || hiv < 0 || hiv > 15) {
                        std::cout << "Error: 個体値は0から15の間で指定してください。" << std::endl;
                    } else if (pokemon_db.find(name) == pokemon_db.end()) {
                        std::cout << "Error: ポケモン '" << name << "' はデータベースに見つかりません。" << std::endl;
                    } else {
                        UserPokemon new_poke;
                        new_poke.id = next_id;
                        new_poke.name = name;
                        new_poke.pl = pl;
                        new_poke.aiv = aiv;
                        new_poke.biv = biv;
                        new_poke.hiv = hiv;
                        new_poke.update_cp();
                        my_box.push_back(new_poke);

                        std::cout << "追加成功: ID=" << new_poke.id
                                  << ", 名前=" << new_poke.name
                                  << ", CP=" << new_poke.cp << std::endl;
                        next_id++;
                    }
                } else {
                    std::cout << "Usage: add pokemon <ポケモン名> <PL> <攻撃個体値> <防御個体値> <HP個体値>" << std::endl;
                }
            } else {
                std::cout << "Unknown add command: " << subcmd << std::endl;
            }
        } else if (cmd == "show") {
            std::string subcmd;
            iss >> subcmd;
            if (subcmd == "pokemon") {
                int id;
                if (iss >> id) {
                    auto it = std::find_if(my_box.begin(), my_box.end(), [id](const UserPokemon& poke) {
                        return poke.id == id;
                    });
                    if (it != my_box.end()) {
                        std::cout << "=== ポケモン詳細情報 ===" << std::endl;
                        std::cout << "ID: " << it->id << " | 名前: " << it->name
                                  << " (メガシンカ: " << (it->active_mega.empty() ? "なし" : it->active_mega) << ")" << std::endl;
                        std::cout << "CP: " << it->cp << " | PL: " << it->pl << std::endl;
                        std::cout << "個体値: 攻撃 " << it->aiv << " / 防御 " << it->biv << " / HP " << it->hiv << std::endl;
                        std::cout << "ノーマル技: " << (it->normalatk.empty() ? "未設定" : it->normalatk) << std::endl;
                        std::cout << "スペシャル技: " << (it->specialatk.empty() ? "未設定" : it->specialatk) << std::endl;
                        std::cout << "サード技: " << (it->thaadatk.empty() ? "未設定" : it->thaadatk) << std::endl;
                        std::cout << "メガ固有技: " << (it->active_mega_move.empty() ? "なし" : it->active_mega_move) << std::endl;
                        std::cout << "特殊状態: シャドウ: " << (it->is_shadow ? "true" : "false")
                                  << " / ライト: " << (it->is_purified ? "true" : "false")
                                  << " / キラ: " << (it->is_lucky ? "true" : "false") << std::endl;
                        std::cout << "相棒ブースト: " << (it->has_buddy_boost ? "true" : "false") << std::endl;
                        std::cout << "メガレベル: " << it->mega_level << std::endl;
                        std::cout << "保護状態: " << (it->is_protected ? "true" : "false") << std::endl;
                    } else {
                        std::cout << "Error: 指定されたIDのポケモンが見つかりません。" << std::endl;
                    }
                } else {
                    std::vector<const UserPokemon*> sorted_box;
                    for (const auto& poke : my_box) {
                        sorted_box.push_back(&poke);
                    }
                    std::sort(sorted_box.begin(), sorted_box.end(), [](const UserPokemon* a, const UserPokemon* b) {
                        return a->cp > b->cp; // CPが高い順
                    });

                    // ヘッダー的なもの（省略してもよいが、表形式っぽく見せるため）
                    // [ID, ポケモン名, CP, PL, 攻撃/防御/HP]
                    for (const auto* poke : sorted_box) {
                        std::cout << "[" << std::setw(3) << std::right << poke->id << ", "
                                  << std::setw(15) << std::left << poke->name << ", "
                                  << "CP: " << std::setw(4) << std::right << poke->cp << ", "
                                  << "PL: " << std::setw(4) << std::left << poke->pl << ", "
                                  << "IVs: " << poke->aiv << "/" << poke->biv << "/" << poke->hiv << "]" << std::endl;
                    }
                    iss.clear(); // 失敗したストリームの状態をクリアする（後続の処理はないが念のため）
                }
            } else {
                std::cout << "Unknown show command: " << subcmd << std::endl;
            }
        } else if (cmd == "delete") {
            std::string subcmd;
            iss >> subcmd;
            if (subcmd == "pokemon") {
                int id;
                if (iss >> id) {
                    auto it = std::find_if(my_box.begin(), my_box.end(), [id](const UserPokemon& poke) {
                        return poke.id == id;
                    });
                    if (it != my_box.end()) {
                        if (it->is_protected) {
                            std::cout << "Error: このポケモンは保護されているため削除できません。" << std::endl;
                        } else {
                            my_box.erase(it);
                            // 削除成功メッセージ等（要件に明記はないが、エラーだけは出すとある）
                        }
                    } else {
                        std::cout << "Error: 指定されたIDのポケモンが見つかりません。" << std::endl;
                    }
                } else {
                    std::cout << "Usage: delete pokemon <id>" << std::endl;
                }
            } else {
                std::cout << "Unknown delete command: " << subcmd << std::endl;
            }
        } else if (cmd == "calc") {
            std::string subcmd;
            iss >> subcmd;
            if (subcmd == "dps") {
                if (my_box.empty()) {
                    std::cout << "Error: ボックスにポケモンがいません。" << std::endl;
                } else {
                    struct DpsRankingEntry {
                        int id;
                        std::string poke_name;
                        std::string fast_move;
                        std::string charge_move;
                        double dps;
                    };

                    std::vector<DpsRankingEntry> ranking;
                    for (const auto& poke : my_box) {
                        auto results = poke.calc_all_effective_dps();
                        std::string display_name = poke.active_mega.empty() ? poke.name : poke.active_mega;
                        for (const auto& res : results) {
                            ranking.push_back({poke.id, display_name, poke.normalatk, res.first, res.second});
                        }
                    }

                    std::sort(ranking.begin(), ranking.end(), [](const DpsRankingEntry& a, const DpsRankingEntry& b) {
                        return a.dps > b.dps;
                    });

                    std::ios_base::fmtflags f(std::cout.flags()); // format state backup
                    int rank = 1;
                    for (const auto& entry : ranking) {
                        std::cout << "[" << std::setw(3) << std::right << rank << "] "
                                  << "ID: " << std::setw(3) << std::right << entry.id << " | "
                                  << std::setw(15) << std::left << entry.poke_name << " | "
                                  << "DPS: " << std::fixed << std::setprecision(2) << std::setw(5) << std::right << entry.dps << " | "
                                  << "技: " << entry.fast_move << " & " << entry.charge_move << std::endl;
                        rank++;
                    }
                    std::cout.flags(f); // restore
                }
            } else {
                std::cout << "Unknown calc command: " << subcmd << std::endl;
            }
        } else if (cmd == "edit") {
            std::string subcmd;
            iss >> subcmd;

            if (subcmd == "pl" || subcmd == "aiv" || subcmd == "biv" || subcmd == "hiv") {
                int id;
                if (iss >> id) {
                    auto it = std::find_if(my_box.begin(), my_box.end(), [id](const UserPokemon& poke) {
                        return poke.id == id;
                    });
                    if (it != my_box.end()) {
                        if (it->is_protected) {
                            std::cout << "Error: このポケモンは保護されているため編集できません。" << std::endl;
                        } else if (subcmd == "pl") {
                            double val;
                            if (iss >> val) {
                                if (val >= 1.0 && val <= 50.0) {
                                    it->pl = val;
                                    it->update_cp();
                                    std::ios_base::fmtflags f(std::cout.flags()); // format state backup
                                    std::cout << it->name << "のPLを" << std::fixed << std::setprecision(1) << val << "に更新しました（現在のCP: " << it->cp << "）" << std::endl;
                                    std::cout.flags(f); // restore
                                } else {
                                    std::cout << "Error: PLは1.0から50.0の間で指定してください。" << std::endl;
                                }
                            } else {
                                std::cout << "Error: 無効な数値フォーマットです。" << std::endl;
                            }
                        } else {
                            int val;
                            if (iss >> val) {
                                if (val >= 0 && val <= 15) {
                                    std::string attr_name;
                                    if (subcmd == "aiv") {
                                        it->aiv = val;
                                        attr_name = "攻撃個体値";
                                    } else if (subcmd == "biv") {
                                        it->biv = val;
                                        attr_name = "防御個体値";
                                    } else if (subcmd == "hiv") {
                                        it->hiv = val;
                                        attr_name = "HP個体値";
                                    }
                                    it->update_cp();
                                    std::cout << it->name << "の" << attr_name << "を" << val << "に更新しました（現在のCP: " << it->cp << "）" << std::endl;
                                } else {
                                    std::cout << "Error: 個体値は0から15の間で指定してください。" << std::endl;
                                }
                            } else {
                                std::cout << "Error: 無効な数値フォーマットです。" << std::endl;
                            }
                        }
                    } else {
                        std::cout << "Error: 指定されたIDのポケモンが見つかりません。" << std::endl;
                    }
                }
            } else if (subcmd == "normalatk" || subcmd == "specialatk" || subcmd == "thaadatk") {
                int id;
                std::string move_name;
                if (iss >> id >> move_name) {
                    auto it = std::find_if(my_box.begin(), my_box.end(), [id](const UserPokemon& poke) {
                        return poke.id == id;
                    });
                    if (it != my_box.end()) {
                        if (it->is_protected) {
                            std::cout << "Error: このポケモンは保護されているため編集できません。" << std::endl;
                        } else if (subcmd == "normalatk") {
                            if (fast_move_db.find(move_name) != fast_move_db.end()) {
                                it->normalatk = move_name;
                                std::cout << it->name << "のノーマルアタックを" << move_name << "に更新しました" << std::endl;
                            } else {
                                std::cout << "Error: わざ '" << move_name << "' はデータベースに見つかりません。" << std::endl;
                            }
                        } else if (subcmd == "specialatk" || subcmd == "thaadatk") {
                            if (charge_move_db.find(move_name) != charge_move_db.end()) {
                                if (subcmd == "specialatk") {
                                    it->specialatk = move_name;
                                    std::cout << it->name << "のスペシャルアタックを" << move_name << "に更新しました" << std::endl;
                                } else {
                                    it->thaadatk = move_name;
                                    std::cout << it->name << "のサードアタックを" << move_name << "に更新しました" << std::endl;
                                }
                            } else {
                                std::cout << "Error: わざ '" << move_name << "' はデータベースに見つかりません。" << std::endl;
                            }
                        }
                    } else {
                        std::cout << "Error: 指定されたIDのポケモンが見つかりません。" << std::endl;
                    }
                } else {
                    std::cout << "Usage: edit " << subcmd << " <id> <わざ名>" << std::endl;
                }
            } else if (subcmd == "shadow" || subcmd == "purified" || subcmd == "lucky" || subcmd == "buddy") {
                int id;
                std::string val_str;
                if (iss >> id >> val_str) {
                    auto it = std::find_if(my_box.begin(), my_box.end(), [id](const UserPokemon& poke) {
                        return poke.id == id;
                    });
                    if (it != my_box.end()) {
                        if (it->is_protected) {
                            std::cout << "Error: このポケモンは保護されているため編集できません。" << std::endl;
                        } else {
                            bool val = false;
                            bool is_valid = true;
                            if (val_str == "true" || val_str == "1") {
                                val = true;
                            } else if (val_str == "false" || val_str == "0") {
                                val = false;
                            } else {
                                std::cout << "Error: 値は true/false または 1/0 で指定してください。" << std::endl;
                                is_valid = false;
                            }

                            if (is_valid) {
                                if (subcmd == "shadow") it->is_shadow = val;
                                else if (subcmd == "purified") it->is_purified = val;
                                else if (subcmd == "lucky") it->is_lucky = val;
                                else if (subcmd == "buddy") it->has_buddy_boost = val;

                                it->update_cp();
                                std::cout << it->name << "の" << subcmd << "を" << (val ? "true" : "false") << "に更新しました（現在のCP: " << it->cp << "）" << std::endl;
                            }
                        }
                    } else {
                        std::cout << "Error: 指定されたIDのポケモンが見つかりません。" << std::endl;
                    }
                } else {
                    std::cout << "Usage: edit " << subcmd << " <id> <true/false>" << std::endl;
                }
            } else if (subcmd == "protection") {
                int id;
                std::string val_str;
                if (iss >> id >> val_str) {
                    auto it = std::find_if(my_box.begin(), my_box.end(), [id](const UserPokemon& poke) {
                        return poke.id == id;
                    });
                    if (it != my_box.end()) {
                        bool val = false;
                        bool is_valid = true;
                        if (val_str == "true" || val_str == "1") {
                            val = true;
                        } else if (val_str == "false" || val_str == "0") {
                            val = false;
                        } else {
                            std::cout << "Error: 値は true/false または 1/0 で指定してください。" << std::endl;
                            is_valid = false;
                        }

                        if (is_valid) {
                            it->is_protected = val;
                            std::cout << it->name << "の保護状態を " << (val ? "true" : "false") << " に更新しました" << std::endl;
                        }
                    } else {
                        std::cout << "Error: 指定されたIDのポケモンが見つかりません。" << std::endl;
                    }
                } else {
                    std::cout << "Usage: edit protection <id> <true/false>" << std::endl;
                }
            } else if (subcmd == "megalevel") {
                int id;
                int val;
                if (iss >> id >> val) {
                    auto it = std::find_if(my_box.begin(), my_box.end(), [id](const UserPokemon& poke) {
                        return poke.id == id;
                    });
                    if (it != my_box.end()) {
                        if (it->is_protected) {
                            std::cout << "Error: このポケモンは保護されているため編集できません。" << std::endl;
                        } else if (val >= 0 && val <= 4) {
                            it->mega_level = val;
                            it->update_cp();
                            std::cout << it->name << "のmegalevelを" << val << "に更新しました（現在のCP: " << it->cp << "）" << std::endl;
                        } else {
                            std::cout << "Error: megalevelは0から4の間で指定してください。" << std::endl;
                        }
                    } else {
                        std::cout << "Error: 指定されたIDのポケモンが見つかりません。" << std::endl;
                    }
                } else {
                    std::cout << "Usage: edit megalevel <id> <0~4>" << std::endl;
                }
            } else if (subcmd == "mega") {
                int id;
                std::string mega_name;
                if (iss >> id >> mega_name) {
                    auto it = std::find_if(my_box.begin(), my_box.end(), [id](const UserPokemon& poke) {
                        return poke.id == id;
                    });
                    if (it != my_box.end()) {
                        if (it->is_protected) {
                            std::cout << "Error: このポケモンは保護されているため編集できません。" << std::endl;
                        } else if (mega_name == "none") {
                            it->active_mega = "";
                            it->active_mega_move = "";
                            it->update_cp();
                            std::cout << "メガシンカを解除しました" << std::endl;
                        } else {
                            if (it->mega_level == 0) {
                                std::cout << "Error: mega_levelが0のポケモンはメガシンカできません。" << std::endl;
                            } else {
                                auto mega_it = mega_db.find(mega_name);
                                if (mega_it == mega_db.end()) {
                                    std::cout << "Error: メガフォーム '" << mega_name << "' はデータベースに見つかりません。" << std::endl;
                                } else if (mega_it->second.base_name != it->name) {
                                    std::cout << "Error: メガフォーム '" << mega_name << "' はこのポケモン (" << it->name << ") のものではありません。" << std::endl;
                                } else {
                                    it->active_mega = mega_name;
                                    it->active_mega_move = mega_it->second.exclusive_move;
                                    it->update_cp();

                                    std::cout << it->name << "は" << mega_name << "にメガシンカしました！";
                                    if (!it->active_mega_move.empty()) {
                                        std::cout << "固有技(" << it->active_mega_move << ")も習得しました。";
                                    }
                                    std::cout << "（現在のCP: " << it->cp << "）" << std::endl;
                                }
                            }
                        }
                    } else {
                        std::cout << "Error: 指定されたIDのポケモンが見つかりません。" << std::endl;
                    }
                } else {
                    std::cout << "Usage: edit mega <id> <メガフォーム名 または none>" << std::endl;
                }
            } else {
                std::cout << "Unknown edit command: " << subcmd << std::endl;
            }
        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
        }

        std::cout << "> ";
    }
    return 0;
}
