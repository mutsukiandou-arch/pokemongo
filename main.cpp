
#include <iostream>
#include <string>
#include <cmath> // sqrt, pow, floor用
#include <unordered_map>
#include <map>

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
extern std::unordered_map<std::string, PokemonBase> pokemon_db;
extern std::map<double, double> cpm_db;

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

