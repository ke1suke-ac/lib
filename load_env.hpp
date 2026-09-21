// 環境変数を変数の型に合わせて読み込むライブラリ
// load_env(name, value): 存在すれば代入してtrue、未設定なら変更せずfalseを返す
// env_value_or(name, default_value): 存在すれば読み込み、未設定ならデフォルト値を返す
// ENV(型, 名前, デフォルト値): 同名の環境変数を読み込んで変数を宣言する
// 対応: bool、64ビットまでの整数型、float、double、long double、std::string
// char系の整数型も数値として扱う。128ビット整数は対応対象外
// 名前は大小文字を区別し、bool値は0/1/false/trueの大小文字を区別しない
// boolは先頭が'1'/'t'/'T'ならtrue、それ以外ならfalseとする簡易変換
// 値は正しい形式・範囲で設定する前提。値の検証や空白の除去は行わない
// std::stringには空文字・空白・大小文字を含め、値全体をコピーする
// env_value_orの型はデフォルト値から推論する。文字列リテラルは<std::string>を指定する
// 計算量のEは環境変数の検索コスト、Lは値・デフォルト値の最大文字数
// 使用例:
//   int width = 100; load_env("WIDTH", width);
//   auto rate = env_value_or("RATE", 0.5);
//   auto mode = env_value_or<std::string>("MODE", "fast");
//   ENV(bool, USE_CACHE, true);
#pragma once
#include <bits/stdc++.h>

template<class T>
bool load_env(const char* name, T& value) {
    const char* s = std::getenv(name);
    if (!s) return false;

    if constexpr (std::is_same_v<T, bool>) {
        value = s[0] == '1' || s[0] == 't' || s[0] == 'T';
    } else if constexpr (std::is_integral_v<T>) {
        if constexpr (std::is_signed_v<T>) value = (T)std::strtoll(s, nullptr, 10);
        else value = (T)std::strtoull(s, nullptr, 10);
    } else if constexpr (std::is_same_v<T, float>) {
        value = std::strtof(s, nullptr);
    } else if constexpr (std::is_same_v<T, double>) {
        value = std::strtod(s, nullptr);
    } else if constexpr (std::is_same_v<T, long double>) {
        value = std::strtold(s, nullptr);
    } else {
        value.assign(s);
    }
    return true;
}

template<class T>
T env_value_or(const char* name, T default_value) {
    load_env(name, default_value);
    return default_value;
}

#define ENV(TYPE, NAME, ...) TYPE NAME = env_value_or<TYPE>(#NAME, (__VA_ARGS__))

#if __INCLUDE_LEVEL__ == 0
#include <source_location>

// グローバルスコープでも通常の変数宣言として使用できる
ENV(int, LOAD_ENV_TEST_GLOBAL, 41);
ENV(std::string, LOAD_ENV_TEST_GLOBAL_TEXT, "global default");

// テスト条件を検証し、失敗した行を表示して停止する / O(1)
void load_env_test_check(bool ok, std::source_location where = std::source_location::current()) {
    if (!ok) {
        std::cerr << where.file_name() << ':' << where.line() << ": 検証失敗\n";
        std::abort();
    }
}

// 整数の境界値・文字列表現・ランダム値を元の値と比較する / O(K(E + L))、Kは検証件数
template<class T>
void load_env_test_integer(std::mt19937_64& rng) {
    // 未設定時は既存値とデフォルト値をそのまま保持する
    const char* name = "LOAD_ENV_TEST_INTEGER";
    load_env_test_check(::unsetenv(name) == 0);
    T value = (T)7;
    load_env_test_check(!load_env(name, value) && value == (T)7);
    load_env_test_check(env_value_or(name, (T)9) == (T)9);

    // 10進文字列へ変換した値が両APIで元に戻ることを確認する
    auto check = [&](T expected) {
        std::string text;
        if constexpr (std::is_signed_v<T>) text = std::to_string((long long)expected);
        else text = std::to_string((unsigned long long)expected);
        load_env_test_check(::setenv(name, text.c_str(), 1) == 0);
        load_env_test_check(load_env(name, value) && value == expected);
        load_env_test_check(env_value_or(name, (T)0) == expected);
    };
    for (T x : {std::numeric_limits<T>::lowest(), std::numeric_limits<T>::max(), (T)0, (T)1}) check(x);
    if constexpr (std::is_signed_v<T>) check((T)-1);
    for (const char* text : {"17", "+17", "00017"}) {
        load_env_test_check(::setenv(name, text, 1) == 0);
        load_env_test_check(load_env(name, value) && value == (T)17);
    }

    // 8ビット型は全値、それ以外も含めて各型2,000個のランダム値を検証する
    if constexpr (sizeof(T) == 1) {
        for (int x = (int)std::numeric_limits<T>::lowest(); x <= (int)std::numeric_limits<T>::max(); ++x) {
            check((T)x);
        }
    }
    for (int i = 0; i < 2000; ++i) {
        if constexpr (std::is_signed_v<T>) {
            std::uniform_int_distribution<long long> dist((long long)std::numeric_limits<T>::lowest(),
                                                          (long long)std::numeric_limits<T>::max());
            check((T)dist(rng));
        } else {
            std::uniform_int_distribution<unsigned long long> dist(0, (unsigned long long)std::numeric_limits<T>::max());
            check((T)dist(rng));
        }
    }
}

// 浮動小数点の境界値・指数表記・ランダム値を元の値と比較する / O(K(E + L))、Kは検証件数
template<class T>
void load_env_test_floating(std::mt19937_64& rng) {
    // 未設定時の維持と、正負のゼロを含む文字列からの復元を確認する
    const char* name = "LOAD_ENV_TEST_FLOATING";
    load_env_test_check(::unsetenv(name) == 0);
    T value = (T)7;
    load_env_test_check(!load_env(name, value) && value == (T)7);
    load_env_test_check(env_value_or(name, (T)9) == (T)9);
    auto check = [&](T expected, const std::string& text) {
        load_env_test_check(::setenv(name, text.c_str(), 1) == 0);
        load_env_test_check(load_env(name, value) && value == expected);
        load_env_test_check(std::signbit(value) == std::signbit(expected));
        T result = env_value_or(name, (T)0);
        load_env_test_check(result == expected && std::signbit(result) == std::signbit(expected));
    };
    auto roundtrip = [&](T expected) {
        std::ostringstream out;
        out << std::setprecision(std::numeric_limits<T>::max_digits10) << expected;
        check(expected, out.str());
    };

    // 極値・非正規化数・型固有の精度と、e/Eの指数表記を確認する
    for (T x : {(T)0, (T)-0.0, (T)1, (T)-1, (T)1 + std::numeric_limits<T>::epsilon(),
                std::numeric_limits<T>::min(), std::numeric_limits<T>::max(),
                std::numeric_limits<T>::lowest(), std::numeric_limits<T>::denorm_min()}) {
        roundtrip(x);
    }
    check((T)0.125, "1.25e-1");
    check((T)0.125, "1.25E-1");
    check((T)-0.125, "-1.25E-1");
    check((T)0.125, "+0.125");
    if constexpr (std::is_same_v<T, float>) {
        check(std::nextafter(1.0f, 2.0f), "1.0000000596046447753906250000000000000001");
    }

    // 十分な桁数で文字列化した各型2,000個のランダム値を比較する
    std::uniform_real_distribution<T> dist((T)-1000000, (T)1000000);
    for (int i = 0; i < 2000; ++i) roundtrip(dist(rng));
}

// 型別の境界値・ランダム値・名前の大小文字・宣言マクロを検証する / O(K(E + L))、Kは検証件数
int main() {
    // 起動前に設定された環境変数がグローバル変数へ反映されることを確認する
    const char* global_int = std::getenv("LOAD_ENV_TEST_GLOBAL");
    const char* global_text = std::getenv("LOAD_ENV_TEST_GLOBAL_TEXT");
    load_env_test_check(LOAD_ENV_TEST_GLOBAL == (global_int ? std::stoi(global_int) : 41));
    load_env_test_check(LOAD_ENV_TEST_GLOBAL_TEXT == (global_text ? global_text : "global default"));

    // 通常の整数型に加え、char系も数値として検証する
    std::mt19937_64 rng(1);
    [&]<class... Ts>(std::tuple<Ts...>) {
        (load_env_test_integer<Ts>(rng), ...);
    }(std::tuple<char, signed char, unsigned char, short, unsigned short, int, unsigned int,
                 long, unsigned long, long long, unsigned long long, wchar_t, char8_t, char16_t, char32_t>{});
    load_env_test_floating<float>(rng);
    load_env_test_floating<double>(rng);
    load_env_test_floating<long double>(rng);

    // boolは未設定・0/1・true/falseの大小文字の全組み合わせを確認する
    const char* bool_name = "LOAD_ENV_TEST_BOOL";
    load_env_test_check(::unsetenv(bool_name) == 0);
    bool flag = true;
    load_env_test_check(!load_env(bool_name, flag) && flag);
    load_env_test_check(env_value_or(bool_name, true));
    load_env_test_check(!env_value_or(bool_name, false));
    for (const char* word : {"0", "1", "false", "true"}) {
        std::string base = word;
        bool expected = base == "1" || base == "true";
        for (unsigned mask = 0; mask < (1u << base.size()); ++mask) {
            std::string text = base;
            for (std::size_t i = 0; i < text.size(); ++i) {
                if ((mask >> i & 1u) && text[i] >= 'a' && text[i] <= 'z') text[i] = (char)(text[i] - 'a' + 'A');
            }
            load_env_test_check(::setenv(bool_name, text.c_str(), 1) == 0);
            flag = !expected;
            load_env_test_check(load_env(bool_name, flag) && flag == expected);
            load_env_test_check(env_value_or(bool_name, !expected) == expected);
        }
    }

    // 文字列は空文字・空白・大小文字・日本語もそのままコピーする
    const char* string_name = "LOAD_ENV_TEST_STRING";
    load_env_test_check(::unsetenv(string_name) == 0);
    std::string value = "default";
    load_env_test_check(!load_env(string_name, value) && value == "default");
    load_env_test_check(env_value_or<std::string>(string_name, "fallback") == "fallback");
    for (const std::string& text : {std::string(), std::string(" Fast\tmode \n"),
                                    std::string("日本語=True"), std::string(4096, 'x')}) {
        load_env_test_check(::setenv(string_name, text.c_str(), 1) == 0);
        load_env_test_check(load_env(string_name, value) && value == text);
        load_env_test_check(env_value_or<std::string>(string_name, "fallback") == text);
        load_env_test_check(::setenv(string_name, "changed", 1) == 0);
        load_env_test_check(value == text);
    }

    // 名前は大小文字を区別し、設定変更・削除後も毎回読み直す
    load_env_test_check(::setenv("LOAD_ENV_TEST_CASE", "12", 1) == 0);
    load_env_test_check(::setenv("load_env_test_case", "34", 1) == 0);
    ENV(int, LOAD_ENV_TEST_CASE, 0);
    ENV(int, load_env_test_case, 0);
    load_env_test_check(LOAD_ENV_TEST_CASE == 12 && load_env_test_case == 34);
    load_env_test_check(::setenv("LOAD_ENV_TEST_CASE", "56", 1) == 0);
    load_env_test_check(load_env("LOAD_ENV_TEST_CASE", LOAD_ENV_TEST_CASE) && LOAD_ENV_TEST_CASE == 56);
    load_env_test_check(::unsetenv("LOAD_ENV_TEST_CASE") == 0);
    load_env_test_check(!load_env("LOAD_ENV_TEST_CASE", LOAD_ENV_TEST_CASE) && LOAD_ENV_TEST_CASE == 56);

    // デフォルト式は設定の有無によらず一度だけ評価し、周囲のローカル変数も使用できる
    int calls = 0;
    auto default_value = [&] { ++calls; return 7; };
    load_env_test_check(::unsetenv("LOAD_ENV_TEST_LOCAL") == 0);
    ENV(int, LOAD_ENV_TEST_LOCAL, default_value());
    load_env_test_check(LOAD_ENV_TEST_LOCAL == 7 && calls == 1);
    load_env_test_check(::setenv("LOAD_ENV_TEST_PRESENT", "42", 1) == 0);
    ENV(int, LOAD_ENV_TEST_PRESENT, default_value());
    load_env_test_check(LOAD_ENV_TEST_PRESENT == 42 && calls == 2);

    // 明示型・文字列リテラル・カンマを含む式・ifの初期化文での宣言を確認する
    load_env_test_check(::setenv("LOAD_ENV_TEST_DOUBLE", "2.5", 1) == 0);
    ENV(double, LOAD_ENV_TEST_DOUBLE, 2);
    load_env_test_check(LOAD_ENV_TEST_DOUBLE == 2.5);
    load_env_test_check(::unsetenv("LOAD_ENV_TEST_TEXT") == 0);
    ENV(std::string, LOAD_ENV_TEST_TEXT, "local default");
    load_env_test_check(LOAD_ENV_TEST_TEXT == "local default");
    load_env_test_check(::unsetenv("LOAD_ENV_TEST_COMMA") == 0);
    ENV(int, LOAD_ENV_TEST_COMMA, std::pair<int, int>{3, 4}.second);
    load_env_test_check(LOAD_ENV_TEST_COMMA == 4);
    load_env_test_check(::unsetenv("LOAD_ENV_TEST_INIT") == 0);
    if (ENV(int, LOAD_ENV_TEST_INIT, 5); LOAD_ENV_TEST_INIT == 5) {
        std::cout << "load_env: all tests passed\n";
    } else {
        load_env_test_check(false);
    }
}
#endif
