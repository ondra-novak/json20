#include "../json/value.h"
#include "../json/parser.h"
#include "../json/serializer.h"

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
                print(iter.key());
                std::cout << ':';
                print(*iter);
                ++iter;
                while (iter != item.end()) {
                    std::cout << ',';
                print(iter.key());
                std::cout << ':';
                print(*iter);
                 ++iter;
                }
            }
            std::cout << "}";
        } else if constexpr(std::is_same_v<T, binary_string_view_t>) {
            std::cout <<  el.as<std::string>();
        } else if constexpr(std::is_same_v<T, json::placeholder_view>) {
            std::cout << "${" << item.position << "}";
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


template<typename Fn>
class TestConstFn {
public:

    constexpr TestConstFn(Fn &&fn) {
        std::string_view txt = fn();
        std::copy(txt.begin(), txt.end(), _buffer);
    }

    std::string_view get_string() const {return {_buffer,len};}


protected:
    static constexpr Fn _dummy_instance;
    static constexpr unsigned int len = std::string_view(_dummy_instance()).length();
    char _buffer[len];

};

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


#if 0
constexpr json::structured_t testjson_structured = []{return json::value{
        {"ahoj","nazdar"},
        {"val",10},
        {"array",{"jedna",2,3.14, json::placeholder<1>}},
        {"object",{
                {"key","value"},
                {"item",123.4567},
                {"pos3", json::placeholder<2>}
        }},
        {"pos1",json::placeholder<0>}
};
};

constexpr json::value testjson = testjson_structured;

constexpr auto test_obj = ([]{return json::object ({
        {"axy",10},
        {"zsee",85},
//        {"sub_test", testjson_structured},
});})();


constexpr TestConstFn test_str = []{return "ahoj lidi";};
#endif
int main() {

    std::vector<int> vect= {1,2,5,3,2,0,10,4,0,4};
    std::sort(json::pair_iterator<int>(vect.data()), json::pair_iterator<int>(vect.data()+vect.size()),
            [](const std::pair<int,int> &a, const std::pair<int,int> &b){
        return a.first < b.first;
    });

    for (auto &x: vect) std::cout << " " << x;
    std::cout << std::endl;

//    auto updated = testjson_structured({10,20,30});


    std::cout << json::value(0.00000012345).to_json() << std::endl;
//    std::cout << test_str.get_string() << std::endl;
  //  std::cout << (test_obj["axy"].as<int>() == 10) << std::endl;
//    std::cout << updated.to_json() << std::endl;
  //  std::cout << test_obj.to_json() << std::endl;

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
    std::string_view k = vtest[1].key();
    std::cout << k << std::endl;
    std::cout << sizeof(json::value) << std::endl;
    json::value vnum(json::number_string("1.2345"));
    json::value vnum2(json::number_string("-123.4545285752485087804896e+03"));
    std::cout << vnum.as<double>() << std::endl;
    std::cout << vnum2.as<double>() << std::endl;
    std::cout << static_cast<int>(vnum.type()) << std::endl;
    std::cout << static_cast<int>(vnum2.type()) << std::endl;

    std::string ss = vtest.to_json();
    std::cout << ss << std::endl;
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
