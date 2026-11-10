
#include <iostream>
#include <string>
#include <cmath> // sqrt, pow, floor用
#include <unordered_map>
#include <map>
#include <vector>
#include <sstream>
#include <algorithm>
#include <iomanip>

// --- 前回設計したマスターデータ用の箱 ---
struct PokemonBase {
    std::string name;
    int attack;
    int defense;
    int hp;
    std::string type1;
    std::string type2;
};

// マスターデータを格納するグローバル変数（メイン関数等でCSVから読み込んでおく）
std::unordered_map<std::string, PokemonBase> pokemon_db;
std::map<double, double> cpm_db;

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
    
    int cp = 0;

    // CPを再計算して更新する関数
    void update_cp() {
        // 1. 種族値をデータベースから検索
        auto poke_it = pokemon_db.find(name);
        if (poke_it == pokemon_db.end()) {
            cp = 0; // エラー時の安全処理
            return;
        }
        const PokemonBase& base = poke_it->second;

        // 2. 実質的なPL（相棒ブーストの処理）
        double effective_pl = pl;
        if (has_buddy_boost) {
            effective_pl += 1.0;
            if (effective_pl > 51.0) effective_pl = 51.0; // 上限はPL51
        }

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
};

// ユーザーの手持ちポケモン管理用
std::vector<UserPokemon> my_box;
int next_id = 1;

int main() {

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
                        my_box.erase(it);
                        // 削除成功メッセージ等（要件に明記はないが、エラーだけは出すとある）
                    } else {
                        std::cout << "Error: 指定されたIDのポケモンが見つかりません。" << std::endl;
                    }
                } else {
                    std::cout << "Usage: delete pokemon <id>" << std::endl;
                }
            } else {
                std::cout << "Unknown delete command: " << subcmd << std::endl;
            }
        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
        }

        std::cout << "> ";
    }
    return 0;
}
