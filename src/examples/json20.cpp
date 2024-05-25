#include <json20.h>

#include <iostream>
#include <iomanip>

namespace JSON20_NAMESPACE_NAME {

void print(const value &el) {
    el.visit([&](const auto &item){
        using T = std::decay_t<decltype(item)>;
        if constexpr(std::is_same_v<T, undefined_t>) {
            std::cout << "undefined";
        } else if constexpr(std::is_same_v<T, array_view>) {
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
        } else if constexpr(std::is_same_v<T, object_view>) {
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
        } else if constexpr(std::is_same_v<T, binary_string_view_t>) {
            std::cout <<  el.as<std::string>();
        } else {
            std::cout <<  item;
        }
    });
}

void println(const value &el) {
    print(el);
    std::cout << std::endl;
}


}

/*

constexpr auto test_json_bin = []{
        auto json = json::value::from_json(test_json);
        json::serializer_t<json::format_t::binary> sr(json);
        json::parser_t<json::format_t::binary> pr;
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
*/
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


constexpr auto testjson_structured = []{return json::structured<[]{return json::value({
        {"ahoj","nazdar"},
        {"val",10},
        {"array",{"jedna",2,3.14}},
        {"object",{
                {"key","value"},
                {"item",123.4567}
        }}
});}>();}();

constexpr const json::value &testjson = testjson_structured;

constexpr auto test_obj = ([]{return json::object ({
        {"axy",10},
        {"zsee",85},
//        {"sub_test", testjson_structured},
});})();


int main() {

    std::cout << (test_obj["axy"].as<int>() == 10) << std::endl;
    std::cout << testjson.to_json() << std::endl;
    std::cout << test_obj.to_json() << std::endl;

    json::value vtest({
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
            {"delete", json::type::undefined},
            {"flags",{{1.258, 12.148e52}}},
            {"array_test",{1,2,3}},
            {"text_contains_quotes","I say \"hello world\"!"},
            {"_real",3.141592},
            {"array",{{"ahoj","nazdar"},json::undefined}},
            {"empty_array",{}}
        });

    json::print({1,2,3,{},4,5,6});
    json::value v = json::object_view({
            {"123456789ABCDEF",1},
            {"bbb",2}
    });
    std::string_view k = v[1].key();
    std::cout << k << std::endl;
    k = vtest[1].key();
    std::cout << k << std::endl;
    std::cout << sizeof(json::value) << std::endl;
    json::value vnum(json::number_string("1.2345"));
    json::value vnum2(json::number_string("-123.4545285752485087804896e+03"));
    std::cout << vnum.as<double>() << std::endl;
    std::cout << vnum2.as<double>() << std::endl;
    std::cout << static_cast<int>(vnum.type()) << std::endl;
    std::cout << static_cast<int>(vnum2.type()) << std::endl;

    std::string ss = vtest.to_json();
    json::parser_t prs;
    json::value out;
    prs.parse(ss.begin(), ss.end(), out);
    print(out);

    std::cout << std::endl;

    json::value smajlik = json::value::from_json(R"("ahoj \uD83D\uDE00")");
    print(smajlik);

    {
        std::string buff;
        json::serializer_t srl;
        srl.serialize_binary(vtest, [&](auto &&c){buff.append(c);});
        hexDump(buff);
    }



    /*
    std::cout << b << ":" << prs.get_parsed().to_json() << std::endl;

    bool parse_json_1 = []{
            const std::string_view txt = "[1,2,true,\"hello\"]extra";
            json::parser_t p;
            p.write(txt);
            auto v = p.get_parsed();
            auto u = p.get_unused_text();
            return true;
    }();

    auto zzz = json::value::from_json(test_json);
    int zzy = test_json_parsed["abc"].as<int>();

    for (char c: example_binary_data) {
        std::cout << c;
    }
    std::cout << std::endl;

    hexDump(to_binary(obj));
*/



	return 0;
}
