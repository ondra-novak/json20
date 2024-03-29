#include "../json20/serializer.h"
#include "../json20/parser.h"
#include <cstdint>

namespace json20 {



constexpr bool check(bool b) {
    if (!b) throw "test failed";
    return b;
}

template<typename T>
constexpr bool print_value_fail(T val) {
    if constexpr(std::is_integral_v<T>) {
        if (val) {
            char fail[1];
            std::ignore = fail[val];
        } else {
            throw "fail: 0";
        }
        return false;
    } else {
        return print_value_fail(static_cast<std::intmax_t>(val));
    }
}

constexpr bool validate_number_1 = []{
    number_string_t a("1.2e3");
    double v = a.parse();
    check(v > 1.199999e3);
    check(v < 1.200001e3);
    check(a.validate());
    return true;
}();

constexpr bool validate_number_2 = []{
    number_string_t a("1.2e3x");
    check(!a.validate());
    return true;
}();

constexpr bool validate_number_3 = []{
    number_string_t a("-12514540654065019080465341874980469408900498403103510609879");
    check(a.validate());
    return true;
}();

constexpr bool validate_number_4 = []{
    number_string_t a("hello");
    check(!a.validate());
    return true;
}();

constexpr bool parse_json_0 = []{
        const std::string_view txt = R"json("aa")json";
        parser_t p;
        p.write(txt);
        auto v = p.get_parsed();
        return true;
}();

constexpr bool parse_json_1 = []{
        const std::string_view txt = "[1,2,true,{\"key\":null,\"key2\":\"world\"},\"hello\"]extra";
        parser_t p;
        p.write(txt);
        auto v = p.get_parsed();
        auto u = p.get_unused_text();
        check(u == "extra");
        check(v[0].as<int>() == 1);
        check(v[1].as<int>() == 2);
        check(v[2].as<bool>() == true);
        check(v[4].as<std::string_view>() == "hello");
        check(v[3].type() == type_t::object);
        check(v[3]["key"] == nullptr);
        check(v[3]["key2"].as<std::string_view>() == "world");
        check(!v[3]["test"]);
        check(!v[3]["aaa"]);
        check(static_cast<bool>(v[3]["key"]));
        int val = v[0].get();
        check(val == 1);
        return true;
}();

constexpr std::string_view test_json = R"json(
{
   "abc":123,
   "xyz":42.42,
   "pole": [1,2,3],
   "2dpole":[
       [1,2,3],
       [4,5,6],
       [7,8,9]],
   "stav":true,
   "stav2":false,
   "nullval":null,
   "text_contains_quotes":"I say \"hello world\"!"
}
)json";



constexpr auto test_json_parsed = []{
        return json20::value_container_t<31,87>(value_t::from_json(test_json));
}();


constexpr auto test_json_bin = []{
        auto json = value_t::from_json(test_json);
        serializer_t<format_t::binary> sr(json);
        parser_t<format_t::binary> pr;
        std::string_view txt = sr.read();
        while (!txt.empty()) {
            if (pr.write(txt)) break;
            txt = sr.read();
        }
        check(json ==  pr.get_parsed());
        return true;
}();


constexpr bool test_base64_encode(std::string_view binary, std::string_view result) {
    std::vector<char> buff;
    base64.encode(binary.begin(), binary.end(), std::back_inserter(buff));
    return result == std::string_view(buff.begin(), buff.end());
}

constexpr bool test_base64_decode(std::string_view text, std::string_view result) {
    std::vector<char> buff;
    base64.decode(text.begin(), text.end(), std::back_inserter(buff));
    return result == std::string_view(buff.begin(), buff.end());
}


constexpr auto test_base64 = []{
        check(test_base64_encode("test","dGVzdA=="));
        check(test_base64_encode("1","MQ=="));
        check(test_base64_encode("~12jio~iuh2JUHBYU[a..,m][2-09šěíáa+é=ě","fjEyamlvfml1aDJKVUhCWVVbYS4uLG1dWzItMDnFocSbw63DoWErw6k9xJs="));
        check(test_base64_encode("ABC","QUJD"));
        check(test_base64_decode("MQ==","1"));
        check(test_base64_decode("QUJD","ABC"));
        check(test_base64_decode("fjEyamlvfml1aDJKVUhCWVVbYS4uLG1dWzItMDnFocSbw63DoWErw6k9xJs=","~12jio~iuh2JUHBYU[a..,m][2-09šěíáa+é=ě"));
        check(test_base64_decode("dGVzdA==","test"));
        check(test_base64_decode("bGlna HQgd29 yay4=","light work."));
        check(test_base64_decode("b-Gln*aHQ(gd)29yaw==","light work"));
        check(test_base64_decode("bGlnaHQgd28=","light wo"));
        return true;
}();


constexpr binary_data example_binary_data("bGlnaHQgd29yay4=");
constexpr unsigned char example_binary_data_decoded[] = {'l','i','g','h','t',' ','w','o','r','k','.'};

constexpr auto test_binary_data = check(static_cast<binary_string_view_t>(example_binary_data) == binary_string_view_t(example_binary_data_decoded, sizeof(example_binary_data_decoded)));


/*

constexpr bool parse_json_2 = []{
        const value_t &v = test_json_parsed;
        check(v["abc"].as<int>() == 123);
        check(v["xyz"].as<int>() == 42);
        check(v["pole"].size() == 3);
        check(v["2dpole"].type() == type_t::array);
        check(v["stav"].as<int>() == 1);
        check(v["stav"].as<std::string_view>() == "true");
        check(v["nullval"] == nullptr);
        check(v["2dpole"].type() == type_t::array);
        check(v["2dpole"][1][1].as<int>() == 5);
        check(!v["2dpole"][1][7]);
        check(!v["2dpole"]["xyz"]);
        check(!v["neexistuje"]);
        check(v["text_contains_quotes"].as<std::string_view>()
             == "I say \"hello world\"!");
        return true;
}();

*/



}




