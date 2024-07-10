//为避免重复声明，通常会利用宏加入 include 防范
//如果宏里有多过一个语句（statement），就需要用 do { /*...*/ } while(0) 包裹成单个语句
#ifndef LEPTJSON_H__
#define LEPTJSON_H__

#include <stddef.h> /* size_t */

//枚举类型（enumeration type）：用项目的简写作为标识符的前缀；枚举值大写，类型或函数小写
typedef enum { LEPT_NULL, LEPT_FALSE, LEPT_TRUE, LEPT_NUMBER, LEPT_STRING, LEPT_ARRAY, LEPT_OBJECT } lept_type;

#define LEPT_KEY_NOT_EXIST ((size_t)-1)

//声明 JSON 的树状数据结构。每个节点使用 lept_value 结构体表示，我们会称它为一个 JSON 值。
//容量——表示当前已分配的元素数目；数量——表示现时的有效元素数目
typedef struct lept_value lept_value;
typedef struct lept_member lept_member;//前向声明
struct lept_value {
    union {
        struct { lept_member* m; size_t size, capacity; }o; /* 对象: 成员, 数量, 容量 */
        struct { lept_value*  e; size_t size, capacity; }a; /* 数组: 元素, 数量, 容量 */
        //用自身类型的指针了，所以必须前向声明此类型
        struct { char* s; size_t len; }s;                   /* 字符串: 字符串, 长度 */
        double n;                                           /* 数字 */
    }u;
    lept_type type;
};
struct lept_member {//lept_value加上键的字符串
    char* k; size_t klen;   //键&长度   加长度是因为字符串本身可能包含空字符 \u0000
    lept_value v;           //值
};


enum {
    LEPT_PARSE_OK = 0,//无错误
    LEPT_PARSE_EXPECT_VALUE,//一个 JSON 只含有空白
    LEPT_PARSE_INVALID_VALUE,//值不是规定的类型
    LEPT_PARSE_ROOT_NOT_SINGULAR,//一个值之后，在空白之后还有其他字符
    LEPT_PARSE_NUMBER_TOO_BIG,
    LEPT_PARSE_MISS_QUOTATION_MARK,
    LEPT_PARSE_INVALID_STRING_ESCAPE,
    LEPT_PARSE_INVALID_STRING_CHAR,
    LEPT_PARSE_INVALID_UNICODE_HEX,
    LEPT_PARSE_INVALID_UNICODE_SURROGATE,
    LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,
    LEPT_PARSE_MISS_KEY,
    LEPT_PARSE_MISS_COLON,
    LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET
};

//由于我们会检查 v 的类型，在调用所有访问函数之前，我们必须设置lept_init函数，初始化该类型。
//用上 do { ... } while(0) 是为了把表达式转为语句，模仿无返回值的函数。
#define lept_init(v) do { (v)->type = LEPT_NULL; } while(0)

//API函数，解析JSON
int lept_parse(lept_value* v, const char* json);
char* lept_stringify(const lept_value* v, size_t* length);

void lept_copy(lept_value* dst, const lept_value* src);
void lept_move(lept_value* dst, lept_value* src);
void lept_swap(lept_value* lhs, lept_value* rhs);

void lept_free(lept_value* v);

lept_type lept_get_type(const lept_value* v);
int lept_is_equal(const lept_value* lhs, const lept_value* rhs);

#define lept_set_null(v) lept_free(v)

int lept_get_boolean(const lept_value* v);
void lept_set_boolean(lept_value* v, int b);

double lept_get_number(const lept_value* v);
void lept_set_number(lept_value* v, double n);

const char* lept_get_string(const lept_value* v);
size_t lept_get_string_length(const lept_value* v);
void lept_set_string(lept_value* v, const char* s, size_t len);

void lept_set_array(lept_value* v, size_t capacity);
size_t lept_get_array_size(const lept_value* v);
size_t lept_get_array_capacity(const lept_value* v);
void lept_reserve_array(lept_value* v, size_t capacity);
void lept_shrink_array(lept_value* v);
void lept_clear_array(lept_value* v);
lept_value* lept_get_array_element(lept_value* v, size_t index);
lept_value* lept_pushback_array_element(lept_value* v);
void lept_popback_array_element(lept_value* v);
lept_value* lept_insert_array_element(lept_value* v, size_t index);
void lept_erase_array_element(lept_value* v, size_t index, size_t count);

void lept_set_object(lept_value* v, size_t capacity);
size_t lept_get_object_size(const lept_value* v);
size_t lept_get_object_capacity(const lept_value* v);
void lept_reserve_object(lept_value* v, size_t capacity);
void lept_shrink_object(lept_value* v);
void lept_clear_object(lept_value* v);
const char* lept_get_object_key(const lept_value* v, size_t index);
size_t lept_get_object_key_length(const lept_value* v, size_t index);
lept_value* lept_get_object_value(lept_value* v, size_t index);
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen);
lept_value* lept_find_object_value(lept_value* v, const char* key, size_t klen);
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen);
void lept_remove_object_value(lept_value* v, size_t index);

#endif /* LEPTJSON_H__ */
