#include "../json20/value.h"
#include "../json20/serializer.h"
#include "../json20/parser.h"

#include <iostream>
#include <iomanip>

namespace json20 {

void print(const value_t &el) {
    el.visit([&](const auto &item){
        using T = std::decay_t<decltype(item)>;
        if constexpr(std::is_same_v<T, undefined_t>) {
            std::cout << "undefined";
        } else if constexpr(std::is_same_v<T, array_t>) {
            std::cout << "[";
            auto iter = item.begin();
            if (iter != item.end()) {
                print(*iter);
                ++iter;
                while (iter != item.end()) {
                    std::cout << ',';
                    print(*iter);
                    ++iter;
                }
            }
            std::cout << "]";
        } else if constexpr(std::is_same_v<T, object_t>) {
            std::cout << "{";
            auto iter = item.begin();
            if (iter != item.end()) {
                print(iter->key);
                std::cout << ':';
                print(iter->value);
                ++iter;
                while (iter != item.end()) {
                    std::cout << ',';
                print(iter->key);
                std::cout << ':';
                print(iter->value);
                 ++iter;
                }
            }
            std::cout << "}";

        } else {
            std::cout <<  item ;
        }
    });
}

void println(const value_t &el) {
    print(el);
    std::cout << std::endl;
}


}

constexpr auto objtst = []{return json20::value_container_t<6>({
    {"aaa",111},
    {"bbb",222},
    {"ccc",333},
});}();

constexpr auto obj = []{return json20::value_container_t<30>({
    {"jmeno","franta"},
    {"prijmeni","voprsalek"},
    {"deti", {
        {
            {"pohlavi", "z"},
            {"jmeno", "jana"},
            {"vek",10},
        }, {
            {"pohlavi","m"},
            {"jmeno", "martin"},
            {"vek",15}
        }
    }},
   {"vek",45},
    {"zenaty", true},
    {"delete", json20::type_t::undefined},
    {"flags",{{1}}}
});}();


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
        return json20::value_container_t<31,87>(json20::value_t::parse(test_json));
}();


constexpr auto test_json_bin = []{
        auto json = json20::value_t::parse(test_json);
        json20::serializer_t<json20::format_t::binary> sr(json);
        json20::parser_t<json20::format_t::binary> pr;
        std::string_view txt = sr.read();
        while (!txt.empty()) {
            if (pr.write(txt)) break;
            txt = sr.read();
        }
        if (pr.is_error()) throw std::runtime_error("error");
        if (json !=  pr.get_parsed()) {
            println(json);
            println(pr.get_parsed());
            throw std::runtime_error("not_same");
        }
        return true;

};

void hexDump(const std::string& input) {
    std::cout << "Hex dump of the string:" << std::endl;

    for (std::size_t i = 0; i < input.length(); ++i) {
        if (i % 16 == 0) {
            if (i != 0)
                std::cout << std::endl;

            std::cout << std::setw(4) << std::setfill('0') << std::hex << i << ": ";
        }

        std::cout << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(static_cast<unsigned char>(input[i])) << ' ';
    }

    std::cout << std::endl;
}

std::string to_binary(json20::value_t v) {
    std::string res;
    json20::serializer_t<json20::format_t::binary> sr(v);
    for (std::string_view str; !(str = sr.read()).empty();) {
        res.append(str);
    }

    return res;
}


int main() {

    test_json_bin();

    println(objtst);

    json20::value_t vtest = {
            {"jmeno","franta"},
            {"prijmeni","voprsalek"},
            {"deti", {
                {
                    {"pohlavi", "z"},
                    {"jmeno", "jana"},
                    {"vek",10},
                }, {
                    {"pohlavi","m"},
                    {"jmeno", "martin"},
                    {"vek",15}
                }
            }},
           {"vek",45},
            {"zenaty", true},
            {"delete", json20::type_t::undefined},
            {"flags",{{1.258, 12.148e52}}}
        };

    json20::print(obj);
    json20::value_t v = json20::object_t({
            {"123456789ABCDEF",1},
            {"bbb",2}
    });
    std::string_view k = v[1].key();
    std::cout << k << std::endl;
    k = vtest[1].key();
    std::cout << k << std::endl;
    std::cout << sizeof(json20::value_t) << std::endl;
    json20::value_t vnum(json20::number_string_t("1.2345"));
    json20::value_t vnum2(json20::number_string_t("-123.4545285752485087804896e+03"));
    std::cout << vnum.as<double>() << std::endl;
    std::cout << vnum2.as<double>() << std::endl;
    std::cout << static_cast<int>(vnum.type()) << std::endl;
    std::cout << static_cast<int>(vnum2.type()) << std::endl;
    std::cout << (vtest == obj) << std::endl;
    std::cout << test_json_parsed.stringify() << std::endl;
    std::cout << obj.to_string() << std::endl;

    std::string ss = vtest.stringify();
    json20::parser_t prs;
    for (char c: ss) {
        prs.write(std::string_view(&c,1));
    }
    bool b = prs.write("");
    //print(prs.get_parsed());
    std::cout << b << ":" << prs.get_parsed().stringify() << std::endl;

    bool parse_json_1 = []{
            const std::string_view txt = "[1,2,true,\"hello\"]extra";
            json20::parser_t p;
            p.write(txt);
            auto v = p.get_parsed();
            auto u = p.get_unused_text();
            return true;
    }();

    auto zzz = json20::value_t::parse(test_json);
    int zzy = test_json_parsed["abc"].as<int>();

    hexDump(to_binary(obj));

	return 0;
}
