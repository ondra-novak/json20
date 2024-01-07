# json20
JSON support for C++20 / constexpr 


# Goals

* JSON-like C++ variable `json20::value_t` capable to store string, number, boolean, object and array.
* support usage in constexpr functions
* constexpr construction of JSON-like structures
* constexpr serializing and parsing
* asynchronous serializing and parsing 
* immutable containers
* optimized variable size (16 bytes to store most of values expect containers)
* small string optimization (up to 15 bytes string is not allocated)
* text numbers - store number (also) in a text form

```
value_t number(123.4);
value_t string("hello");
value_t array({1,2,3,4,5};
value_t object({
    {"first name","Ondra"},
    {"last name","Novak"},
    {"age",47},
});

int n = number.get();
auto text = string.as<std::string_view>();
for (const auto &x: array) {
    std::cout<<x.as<int>() << std::endl;
}
for (const auto &x: object) {
    std::cout<<x.ket() << ":" << x.to_string() << std::endl;
}

```


## value_container_t 

It is storage fo constexpr constructed JSON-like structure

```
value_container_t<Elements> c1;
value_container_t<Elements, StringBuffer> c2;
```

* `Elements` - count of elements, because the c++20 is unable to calculate this value, you need to guess it the compiler checks, whether it is correct guess. It is also able to show the correct value.
* `StringBuffer` - size of string buffer. It must be specified, when the
constant is constructed using `value_t::parse()`, because strings
must be stored somewhere. This value must be also guessed for the first time

**note** - gcc requires construction through a lambda function

```
constexpr auto cobject = []{
    return value_container_t<6>({ 
        {"first name","Ondra"},
        {"last name","Novak"},
        {"age",47},
    });
}();
```

The `Elements` number is 6, because 6 extra elemnts are reserved for
3 values and 3 keys. No string buffer is needed, because strings are
constexpr in this case

```

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
        return json20::value_container_t<31,87>(value_t::parse(test_json));
}();
```
There is `31` elements in total, and  we also need `87` bytes to store
strings. Note that floating numbers are stored as strings.

## serializer_t

Initialize serializer

```
value_t v = {1,2,3}
serializer_t srl(v);
```

Read result until empty string is returned

```
for(std::string_view str;!(str = srl.read()).empty();) {
    std::cout << str;
}
```

The output is generated **per element**, this allows to use serializer
as part asynchrouns IO.

## parser_t

Initialize parser

```
parser_t prs;
```

Write string string. You can write it once, or per partes

```
prs.write(...);
prs.write(...);
prs.write(...);
prs.write(...);
```

Each call returns `true` when output is ready, or `false` when need more data

```
std::string_view s = io.read();
while (!prs.write(s)) {
     s = io.read();
}
```

You should finish write by sending empty string, but this isn't necessery when output is ready.

To retrieve output, you call `pr.get_parsed()`

```
value_t val = pr.get_parsed();
```

You should also check for errors. If error is detected, it is reported as `output is ready` however you need to call `is_error` to check, whether
an error has been detected


```
if (pr.is_error()) throw std::runtime_error("parser error");
value_t val = pr.get_parsed();
```

It can happen, that some written text was unused for the result and it
can be part of next message. You can retrieve unused text by calling

```
std::string_view unused = pr.get_unused_text()
```




