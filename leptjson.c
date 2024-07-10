//JSON是一个用于交换的数据格式。例如，一个动态网页想从服务器获得数据时，服务器从数据库查找数据，然后把数据转换成 JSON 文本格式：
//{
//    "title": "Design Patterns",
//        "subtitle" : "Elements of Reusable Object-Oriented Software",
//        "author" : [
//            "Erich Gamma",
//                "Richard Helm",
//                "Ralph Johnson",
//                "John Vlissides"
//        ] ,
//        "year": 2009,
//        "weight" : 1.8,
//        "hardcover" : true,
//        "publisher" : {
//        "Company": "Pearson Education",
//            "Country" : "India"
//    },
//        "website" : null
//}
//JSON 是树状结构，含 6 种数据类型：null,boolean(true|false),number浮点数,string"...",array[...],object{ ... }
//实现JSON库，主要是完成3需求：JSON文本解析parse为树状数据结构；提供接口access访问该树状数据结构；把树状数据结构转换stringfy为JSON文本

//断言assert是 C 语言中常用的防御式编程方式，减少编程错误。
// 最常用的是在函数开始的地方，检测所有参数。有时候也可以在调用函数后，检查上下文是否正确。
//当程序以 release 配置编译时，assert() 不会做检测； 
// debug 配置时会检测 assert() 中条件是否为真，若失败则程序崩溃。


#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif
#include "leptjson.h"
#include <assert.h>  /* assert() */
#include <errno.h>   /* errno, ERANGE */
#include <math.h>    /* HUGE_VAL */
#include <stdio.h>   /* sprintf() */
#include <stdlib.h>  /* NULL, malloc(), realloc(), free(), strtod() */
#include <string.h>  /* memcpy() */


#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
#define PUTC(c, ch)         do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)
#define PUTS(c, s, len)     memcpy(lept_context_push(c, len), s, len)

//堆栈是以字节储存的。每次可要求压入任意大小的数据，它会返回数据起始的指针
//把初始大小以宏 LEPT_PARSE_STACK_INIT_SIZE 的形式定义。
//使用 #ifndef X #define X ... #endif 方式的好处是，使用者可在编译选项中自行设置宏，没设置的话就用缺省值。
#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif

#ifndef LEPT_PARSE_STRINGIFY_INIT_SIZE
#define LEPT_PARSE_STRINGIFY_INIT_SIZE 256
#endif

//为了减少解析函数之间传递多个参数，我们把这些数据都放进一个 lept_context 结构体
typedef struct {
    const char* json;
    char* stack;
    size_t size, top;
}lept_context;



// json 堆栈压入与弹出
//压入时若空间不足，便回以 1.5 倍大小扩展。
//用 realloc() 来重新分配内存。
static void* lept_context_push(lept_context* c, size_t size) {
    void* ret;
    assert(size > 0);
    if (c->top + size >= c->size) {
        if (c->size == 0)   c->size = LEPT_PARSE_STACK_INIT_SIZE;
        while (c->top + size >= c->size)
            c->size += c->size >> 1;  /* c->size * 1.5 */
        c->stack = (char*)realloc(c->stack, c->size);
    }
    ret = c->stack + c->top;
    c->top += size;
    return ret;
}
static void* lept_context_pop(lept_context* c, size_t size) {
    assert(c->top >= size);
    return c->stack + (c->top -= size);
}

// json whitespace 解析   
//注：此函数是不会出现错误的，不返回错误码，所以返回类型为 void。
static void lept_parse_whitespace(lept_context* c) {
    const char *p = c->json;
    //遇到空格、制表、换行、回车 就前进
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
        p++;
    c->json = p;
}

//lept_parse_null(c,v)     遇到n就判断是不是"null"，是则c进3|v改null|返回ok，不是则报错
//static int lept_parse_null(lept_context* c, lept_value* v) {
//    EXPECT(c, 'n');    //看是否合法，并跳到下一字符
//    if (c->json[0] != 'u' || c->json[1] != 'l' || c->json[2] != 'l')   return LEPT_PARSE_INVALID_VALUE;
//    c->json += 3;
//    v->type = LEPT_NULL;
//    return LEPT_PARSE_OK;
//}
//static int lept_parse_true(lept_context* c, lept_value* v)同理
//static int lept_parse_false(lept_context* c, lept_value* v)同理
//practice三者合并                         (c,             v,             "true",       LEPT_TRUE)
static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type) {
    size_t i;
    EXPECT(c, literal[0]);
    for (i = 0; literal[i + 1]; i++)
        if (c->json[i] != literal[i + 1])
            return LEPT_PARSE_INVALID_VALUE;
    c->json += i;
    v->type = type;
    return LEPT_PARSE_OK;
}


// json number 解析
static int lept_parse_number(lept_context* c, lept_value* v) {
    const char* p = c->json;   // 用一个指针 p 来表示当前的解析字符位置。校验成功，才把 p 赋值至 c->json。
    //负号：跳过
    if (*p == '-') p++;
    //整数: 0 开始，只能是单个 0;    1-9 开始，可以加任意数量的数字（0-9）
    if (*p == '0') p++;
    else {
        if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);//先把指针p进一位；如果p指向的是数字就继续进位
    }
    //小数 :小数点后是一或多个数字（0-9）
    if (*p == '.') {
        p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);//先把指针p进一位；如果p指向的是数字就继续进位
    }
    //指数:由大写 E 或小写 e 开始，然后可有正负号，之后是一或多个数字
    if (*p == 'e' || *p == 'E') {
        p++;
        if (*p == '+' || *p == '-') p++;
        if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
        for (p++; ISDIGIT(*p); p++);
    }
    //数字过大的处理   #include <errno.h>   #include <math.h>
    errno = 0;
    v->u.n = strtod(c->json, NULL);  //十进制通过strtod()转二进制 double
    if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))  return LEPT_PARSE_NUMBER_TOO_BIG;


    v->type = LEPT_NUMBER;
    c->json = p;
    return LEPT_PARSE_OK;
}

// json 十六进制数 解析
static const char* lept_parse_hex4(const char* p, unsigned* u) {
    *u = 0;
    for (int i = 0; i < 4; i++) {
        char ch = *p++;
        *u <<= 4;
        if      (ch >= '0' && ch <= '9')  *u |= ch - '0';
        else if (ch >= 'A' && ch <= 'F')  *u |= ch - ('A' - 10);
        else if (ch >= 'a' && ch <= 'f')  *u |= ch - ('a' - 10);
        else return NULL;
    }
    return p;
}

//通过码点编码字节
static void lept_encode_utf8(lept_context* c, unsigned u) {
    if (u <= 0x7F) 
        PUTC(c, u & 0xFF);
    else if (u <= 0x7FF) {
        PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
        PUTC(c, 0x80 | ( u       & 0x3F));
    }
    else if (u <= 0xFFFF) {
        PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
    else {
        assert(u <= 0x10FFFF);
        PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
        PUTC(c, 0x80 | ((u >> 12) & 0x3F));
        PUTC(c, 0x80 | ((u >>  6) & 0x3F));
        PUTC(c, 0x80 | ( u        & 0x3F));
    }
}

//字符串出错了就把指针调回栈首，并返回错误码
#define STRING_ERROR(ret) do { c->top = head; return ret; } while(0)

// json 字符串解析   用json对象重构
//代码重构是指修改代码以改进结构。它十分依赖于单元测试。通过单元测试重构，寻找最佳改动。
//成员的键也是一个 JSON 字符串，然而，我们不使用 lept_value 存储键，因为这样会浪费了当中 type 这个无用的字段。
//原 lept_parse_string() 直接把字符串解析的结果写入 lept_value。
//我们用「提取方法」的重构方式，把字符串解析&写入 lept_value 分拆成两部分：
static int lept_parse_string_raw(lept_context* c, char** str, size_t* len) {//字符串解析，结果写入str和len
    size_t head = c->top;//备份栈顶
    unsigned u, u2;
    const char* p;
    EXPECT(c, '\"');
    p = c->json;
    for (;;) {
        char ch = *p++;
        switch (ch) {
            case '\"':
                *len = c->top - head;//计算长度
                *str = lept_context_pop(c, *len);//字符出栈
                c->json = p;
                return LEPT_PARSE_OK;
            case '\\':  //出现转义字符
                switch (*p++) {
                    case '\"': PUTC(c, '\"'); break; //字符入栈
                    case '\\': PUTC(c, '\\'); break;
                    case '/':  PUTC(c, '/' ); break;
                    case 'b':  PUTC(c, '\b'); break;
                    case 'f':  PUTC(c, '\f'); break;
                    case 'n':  PUTC(c, '\n'); break;
                    case 'r':  PUTC(c, '\r'); break;
                    case 't':  PUTC(c, '\t'); break;
                    case 'u':  //  \uxxxx转义字符
                        if (!(p = lept_parse_hex4(p, &u)))//十六进制数不合法
                            STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                        if (u >= 0xD800 && u <= 0xDBFF) { /* surrogate pair */
                            if (*p++ != '\\')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if (*p++ != 'u')
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            if (!(p = lept_parse_hex4(p, &u2)))
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
                            if (u2 < 0xDC00 || u2 > 0xDFFF)
                                STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
                            u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
                        }
                        lept_encode_utf8(c, u);
                        break;
                    default:
                        STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
                }
                break;
            case '\0':
                STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);
            default:
                if ((unsigned char)ch < 0x20)
                    STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
                PUTC(c, ch);
        }
    }
}
static int lept_parse_string(lept_context* c, lept_value* v) {//解析结果写入lept_value，复制至 lept_member 的 k 和 klen 字段
    int ret;
    char* s;
    size_t len;
    if ((ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK)
        lept_set_string(v, s, len);
    return ret;//=lept_parse_string_raw(c, &s, &len)
}

static int lept_parse_value(lept_context* c, lept_value* v);

//JSON 数组的语法：
//array = % x5B ws[value * (ws % x2C ws value)] ws % x5D
//% x5B 是左中括号[，
//% x2C 是逗号, ，
//% x5D 是右中括号] ，
//ws 是空白字符。
//一个数组可以包含零至多个值，以逗号分隔，例如[]、[1, 2, true]、 [[1, 2], [3, 4], "abc"] 都是合法的数组。
// 但注意 JSON 不接受末端额外的逗号，例如[1, 2, ] 是不合法的
//
// JSON 数组 进行解析
//用解析 JSON 字符串时相同的方法【临时缓冲区】，而且可以用同一个堆栈！
//把每个解析好的元素入栈，解析到数组结束时，所有元素出栈，复制至新分配的内存中。
//
//如果把 JSON 当作一棵树的数据结构，JSON 字符串是叶节点，而 JSON 数组是中间节点。
//在叶节点的解析函数中，我们怎样使用那个堆栈也可以，只要最后还原就好了。
//但对于数组这样的中间节点，共用这个堆栈没问题么？
//答案是：只要在解析函数结束时还原堆栈的状态，就没有问题。
static int lept_parse_array(lept_context* c, lept_value* v) {
    size_t i, size = 0;
    int ret;
    EXPECT(c, '[');
    lept_parse_whitespace(c);
    if (*c->json == ']') {//空数组
        c->json++;
        lept_set_array(v, 0);
        return LEPT_PARSE_OK;
    }
    for (;;) {
        lept_value e;
        lept_init(&e);
        if ((ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK)//发现不合法break
            break;
        memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));//解析好了的元素入栈
        size++;
        lept_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        }
        else if (*c->json == ']') {//数组结束
            c->json++;
            lept_set_array(v, size);
            memcpy(v->u.a.e, lept_context_pop(c, size * sizeof(lept_value)), size * sizeof(lept_value));
            v->u.a.size = size;
            return LEPT_PARSE_OK;
        }
        else {
            ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;
            break;
        }
    }
    /* Pop and free values on the stack */
    for (i = 0; i < size; i++)
        lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));//所有元素出栈，复制到新分配的内存中
    return ret;//= lept_parse_value(c, &e)
}

// json 对象 进行解析
//JSON 对象和 JSON 数组非常相似，区别包括 JSON 对象以花括号 {}包裹表示，
//JSON 对象由对象成员组成，而 JSON 数组由 JSON 值组成。
//所谓对象成员，就是键值对，键必须为 JSON 字符串，值是任何 JSON 值，中间以冒号:分隔
static int lept_parse_object(lept_context* c, lept_value* v) {
    size_t i, size;
    lept_member m;
    int ret;
    EXPECT(c, '{');
    lept_parse_whitespace(c);
    if (*c->json == '}') {
        //空对象
        c->json++;
        lept_set_object(v, 0);
        return LEPT_PARSE_OK;
    }
    m.k = NULL;
    size = 0;
    for (;;) {
        char* str;
        lept_init(&m.v);
        /* 解析键 */
        if (*c->json != '"') {
            ret = LEPT_PARSE_MISS_KEY;
            break;
        }
        if ((ret = lept_parse_string_raw(c, &str, &m.klen)) != LEPT_PARSE_OK)
            break;
        memcpy(m.k = (char*)malloc(m.klen + 1), str, m.klen);
        m.k[m.klen] = '\0';/* 键入栈 */
        /* 解析冒号 */
        lept_parse_whitespace(c);
        if (*c->json != ':') {
            ret = LEPT_PARSE_MISS_COLON;
            break;
        }
        c->json++;
        lept_parse_whitespace(c);
        /* 解析值 */
        if ((ret = lept_parse_value(c, &m.v)) != LEPT_PARSE_OK)
            break;
        memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member));
        size++;
        m.k = NULL; /* 值入栈 */
        /* 解析逗号与右花括号 */
        lept_parse_whitespace(c);
        if (*c->json == ',') {
            c->json++;
            lept_parse_whitespace(c);
        }
        else if (*c->json == '}') {
            c->json++;
            lept_set_object(v, size);
            memcpy(v->u.o.m, lept_context_pop(c, sizeof(lept_member) * size), sizeof(lept_member) * size);
            v->u.o.size = size;
            return LEPT_PARSE_OK;
        }
        else {
            ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;
            break;
        }
    }
    /* 输出栈内元素 */
    free(m.k);
    for (i = 0; i < size; i++) {
        lept_member* m = (lept_member*)lept_context_pop(c, sizeof(lept_member));
        free(m->k);
        lept_free(&m->v);
    }
    v->type = LEPT_NULL;
    return ret;
}

// leptjson 是一个手写的递归下降解析器。我们只需检测下一个字符，便可以知道它是哪种类型的值，再调用相关的分析函数。
// 对于完整的 JSON 语法，跳过空白后，只需检测当前字符：
// n ➔ null
// t ➔ true
// f ➔ false
// " ➔ string
// 0-9/- ➔ number
// [ ➔ array
// { ➔ object
static int lept_parse_value(lept_context* c, lept_value* v) {
    switch (*c->json) {
        case 't':  return lept_parse_literal(c, v, "true", LEPT_TRUE);
        case 'f':  return lept_parse_literal(c, v, "false", LEPT_FALSE);
        case 'n':  return lept_parse_literal(c, v, "null", LEPT_NULL);
        default:   return lept_parse_number(c, v);
        case '"':  return lept_parse_string(c, v);
        case '[':  return lept_parse_array(c, v);
        case '{':  return lept_parse_object(c, v);
        case '\0': return LEPT_PARSE_EXPECT_VALUE;
    }
}

// JSON 文本 进行解析
//JSON文本语法：ws value ws  空白+值+空白
// 传入的 JSON 文本是一个空结尾字符串，该输入字符串固定不变，故使用 const char* 类型。
// 传入的根节点指针 v 是由使用方负责分配的，所以一般用法是：
// lept_value v;
// const char json[] = ...;
// int ret = lept_parse(&v, json) = 各种报错值
int lept_parse(lept_value* v, const char* json) {
    lept_context c;
    int ret;
    assert(v != NULL);

    //初始化
    c.json = json;
    c.stack = NULL;
    c.size = c.top = 0;
    lept_init(v);//v->type = LEPT_NULL;

    lept_parse_whitespace(&c);//解析空白，然后检查 JSON 文本是否完结
    //让 lept_parse_value() 把解析出来的根值写入ret
    if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK) {
        lept_parse_whitespace(&c);
        if (*c.json != '\0') 
        {   //若 json 在一个值之后，空白之后还有其它字符，则要返回 LEPT_PARSE_ROOT_NOT_SINGULAR。
            v->type = LEPT_NULL;
            ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
        }
    }

    //释放内存，加入断言确保所有数据都被弹出
    assert(c.top == 0);
    free(c.stack);
    return ret;
}

static void lept_stringify_string(lept_context* c, const char* s, size_t len) {
    static const char hex_digits[] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    size_t i, size;
    char* head, *p;
    assert(s != NULL);
    p = head = lept_context_push(c, size = len * 6 + 2); /* "\u00xx..." */
    *p++ = '"';
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)s[i];
        switch (ch) {
            case '\"': *p++ = '\\'; *p++ = '\"'; break;
            case '\\': *p++ = '\\'; *p++ = '\\'; break;
            case '\b': *p++ = '\\'; *p++ = 'b';  break;
            case '\f': *p++ = '\\'; *p++ = 'f';  break;
            case '\n': *p++ = '\\'; *p++ = 'n';  break;
            case '\r': *p++ = '\\'; *p++ = 'r';  break;
            case '\t': *p++ = '\\'; *p++ = 't';  break;
            default:
                if (ch < 0x20) {
                    *p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
                    *p++ = hex_digits[ch >> 4];
                    *p++ = hex_digits[ch & 15];
                }
                else
                    *p++ = s[i];
        }
    }
    *p++ = '"';
    c->top -= size - (p - head);
}

//JSON生成器 对输入值进行解析
static void lept_stringify_value(lept_context* c, const lept_value* v) {
    size_t i;
    switch (v->type) {
        case LEPT_NULL:   PUTS(c, "null",  4); break;
        case LEPT_FALSE:  PUTS(c, "false", 5); break;
        case LEPT_TRUE:   PUTS(c, "true",  4); break;
        /*为了简单起见，我们使用 sprintf("%.17g", ...) 来把浮点数转换成文本。"%.17g" 足够把双精度浮点转换成可还原的文本。
        case LEPT_NUMBER:
            {
                char buffer[32];
                int length = sprintf(buffer, "%.17g", v->u.n);
                PUTS(c, buffer, length);
            }
            break;
        但这样需要在 PUTS() 中做一次 memcpy()，实际上我们可以避免这次复制，只需要生成的时候直接写进 c 里的堆栈，然后再按实际长度调查 c->top：
        case LEPT_NUMBER:
            {
                char* buffer = lept_context_push(c, 32);
                int length = sprintf(buffer, "%.17g", v->u.n);
                c->top -= 32 - length;
            }
            break;
        因每个临时变量只用了一次，我们可以把代码压缩成一行：*/
        case LEPT_NUMBER: c->top -= 32 - sprintf(lept_context_push(c, 32), "%.17g", v->u.n); break;
        case LEPT_STRING: lept_stringify_string(c, v->u.s.s, v->u.s.len); break;
        case LEPT_ARRAY:
            PUTC(c, '[');
            for (i = 0; i < v->u.a.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                lept_stringify_value(c, &v->u.a.e[i]);
            }
            PUTC(c, ']');
            break;
        case LEPT_OBJECT:
            PUTC(c, '{');
            for (i = 0; i < v->u.o.size; i++) {
                if (i > 0)
                    PUTC(c, ',');
                lept_stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);
                PUTC(c, ':');
                lept_stringify_value(c, &v->u.o.m[i].v);
            }
            PUTC(c, '}');
            break;
        default: assert(0 && "invalid type");
    }
}

//JSON 生成器把树形数据结构转换成 JSON 文本。这个过程又称为「字符串化」。
//JSON生成器直接返回JSON字符串   length 参数是可选的，传入 NULL 可忽略此参数。当传入非空指针时，就能获得生成 JSON 的长度。strlen()获取长度消耗大。
//使用方需负责用 free() 释放内存
//在解析 JSON 时，用堆栈存储临时的解析结果。这里要要存储生成的结果，所以应该再利用该数据结构作为输出缓冲区。
char* lept_stringify(const lept_value* v, size_t* length) {
    lept_context c;
    assert(v != NULL);
    c.stack = (char*)malloc(c.size = LEPT_PARSE_STRINGIFY_INIT_SIZE);
    c.top = 0;
    lept_stringify_value(&c, v);
    if (length)
        *length = c.top;
    PUTC(&c, '\0');//生成根节点的值之后，我们还需要加入一个空字符作结尾
    return c.stack;
}

//深度复制函数
void lept_copy(lept_value* dst, const lept_value* src) {
    assert(src != NULL && dst != NULL && src != dst);
    switch (src->type) {
        case LEPT_STRING:
            lept_set_string(dst, src->u.s.s, src->u.s.len);
            break;
        case LEPT_ARRAY:
            /* \todo */
            break;
        case LEPT_OBJECT:
            /* \todo */
            break;
        default:
            lept_free(dst);
            memcpy(dst, src, sizeof(lept_value));
            break;
    }
}

void lept_move(lept_value* dst, lept_value* src) {
    assert(dst != NULL && src != NULL && src != dst);
    lept_free(dst);
    memcpy(dst, src, sizeof(lept_value));
    lept_init(src);
}

void lept_swap(lept_value* lhs, lept_value* rhs) {
    assert(lhs != NULL && rhs != NULL);
    if (lhs != rhs) {
        lept_value temp;
        memcpy(&temp, lhs, sizeof(lept_value));
        memcpy(lhs,   rhs, sizeof(lept_value));
        memcpy(rhs, &temp, sizeof(lept_value));
    }
}

//把v变成null值
void lept_free(lept_value* v) {
    size_t i;
    assert(v != NULL);
    switch (v->type) {
        case LEPT_STRING:
            free(v->u.s.s);
            break;
        case LEPT_ARRAY:
            for (i = 0; i < v->u.a.size; i++)
                lept_free(&v->u.a.e[i]);
            free(v->u.a.e);
            break;
        case LEPT_OBJECT:
            for (i = 0; i < v->u.o.size; i++) {
                free(v->u.o.m[i].k);
                lept_free(&v->u.o.m[i].v);
            }
            free(v->u.o.m);
            break;
        default: break;
    }
    v->type = LEPT_NULL;
}

//获取结果的类型
lept_type lept_get_type(const lept_value* v) {
    assert(v != NULL);
    return v->type;
}

// 在实现数组和对象的修改之前，为了测试结果的正确性，我们先实现 lept_value 的相等比较。
// 首先，两个值的类型必须相同，
// 对于 true、false、null 这三种类型，比较类型后便完成比较。
// 而对于数字和字符串，需进一步检查是否相等：
int lept_is_equal(const lept_value* lhs, const lept_value* rhs) {
    size_t i;
    assert(lhs != NULL && rhs != NULL);
    if (lhs->type != rhs->type)//两值类型不同，不行
        return 0;
    switch (lhs->type) {
        case LEPT_STRING:
            return lhs->u.s.len == rhs->u.s.len && 
                memcmp(lhs->u.s.s, rhs->u.s.s, lhs->u.s.len) == 0;
        case LEPT_NUMBER:
            return lhs->u.n == rhs->u.n;
        // 由于值可能是复合类型（数组和对象），也就是一个树形结构。
        // 当我们要比较两个树是否相等，可通过递归实现。
        // 例如，对于数组，我们先比较元素数目是否相等，然后递归检查对应的元素是否相等：
        case LEPT_ARRAY:
            if (lhs->u.a.size != rhs->u.a.size)//比较元素数目是否相等
                return 0;
            for (i = 0; i < lhs->u.a.size; i++)
                if (!lept_is_equal(&lhs->u.a.e[i], &rhs->u.a.e[i]))//递归检查对应的元素是否相等
                    return 0;
            return 1;
        // 对象的键值对是无序的。
        // 例如，{"a":1,"b":2} 和 {"b":2,"a":1} 虽然键值的次序不同，但这两个 JSON 对象是相等的。
        // 我们可以简单地利用 lept_find_object_index() 去找出对应的值，然后递归作比较。
        case LEPT_OBJECT:
            /* \todo */
            return 1;
        default:
            return 1;
    }
}

int lept_get_boolean(const lept_value* v) {
    assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
    return v->type == LEPT_TRUE;
}

void lept_set_boolean(lept_value* v, int b) {
    lept_free(v);
    v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

//仅当 type == LEPT_NUMBER 时，n 才表示 JSON 数字的数值。所以获取该值的 API 是这么实现的：
double lept_get_number(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_NUMBER);
    return v->u.n;
}

void lept_set_number(lept_value* v, double n) {
    lept_free(v);
    v->u.n = n;
    v->type = LEPT_NUMBER;
}


const char* lept_get_string(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.s;
}

size_t lept_get_string_length(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_STRING);
    return v->u.s.len;
}

//由于字符串的长度不是固定的，我们要动态分配内存。用malloc()、realloc() 和 free() 来分配／释放内存。
//当设置一个值为字符串时，我们需要把参数中的字符串复制一份：
void lept_set_string(lept_value* v, const char* s, size_t len) {
    assert(v != NULL && (s != NULL || len == 0));
    lept_free(v);
    v->u.s.s = (char*)malloc(len + 1);
    memcpy(v->u.s.s, s, len);
    v->u.s.s[len] = '\0';
    v->u.s.len = len;
    v->type = LEPT_STRING;
}

void lept_set_array(lept_value* v, size_t capacity) {//初始化数组
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_ARRAY;
    v->u.a.size = 0;
    v->u.a.capacity = capacity;
    v->u.a.e = capacity > 0 ? (lept_value*)malloc(capacity * sizeof(lept_value)) : NULL;
}

size_t lept_get_array_size(const lept_value* v) {//得数组大小
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.size;
}

size_t lept_get_array_capacity(const lept_value* v) {//得数组容量
    assert(v != NULL && v->type == LEPT_ARRAY);
    return v->u.a.capacity;
}

void lept_reserve_array(lept_value* v, size_t capacity) {//如果当前的容量不足，我们需要扩大容量，标准库的 realloc() 可以分配新的内存并把旧的数据拷背过去
    assert(v != NULL && v->type == LEPT_ARRAY);
    if (v->u.a.capacity < capacity) 
    {
        v->u.a.capacity = capacity;
        v->u.a.e = (lept_value*)realloc(v->u.a.e, capacity * sizeof(lept_value));
    }
}

void lept_shrink_array(lept_value* v) {//当数组不需要再修改，可以使用以下的函数，把容量缩小至刚好能放置现有元素：
    assert(v != NULL && v->type == LEPT_ARRAY);
    if (v->u.a.capacity > v->u.a.size) 
    {
        v->u.a.capacity = v->u.a.size;
        v->u.a.e = (lept_value*)realloc(v->u.a.e, v->u.a.capacity * sizeof(lept_value));
    }
}

void lept_clear_array(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    lept_erase_array_element(v, 0, v->u.a.size);
}

lept_value* lept_get_array_element(lept_value* v, size_t index) {//得数组元素
    assert(v != NULL && v->type == LEPT_ARRAY);
    assert(index < v->u.a.size);
    return &v->u.a.e[index];
}

lept_value* lept_pushback_array_element(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY);
    if (v->u.a.size == v->u.a.capacity)
        lept_reserve_array(v, v->u.a.capacity == 0 ? 1 : v->u.a.capacity * 2);
    lept_init(&v->u.a.e[v->u.a.size]);
    return &v->u.a.e[v->u.a.size++];
}

void lept_popback_array_element(lept_value* v) {
    assert(v != NULL && v->type == LEPT_ARRAY && v->u.a.size > 0);
    lept_free(&v->u.a.e[--v->u.a.size]);
}

lept_value* lept_insert_array_element(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_ARRAY && index <= v->u.a.size);
    /* \todo */
    return NULL;
}

void lept_erase_array_element(lept_value* v, size_t index, size_t count) {
    assert(v != NULL && v->type == LEPT_ARRAY && index + count <= v->u.a.size);
    /* \todo */
}

void lept_set_object(lept_value* v, size_t capacity) {
    assert(v != NULL);
    lept_free(v);
    v->type = LEPT_OBJECT;
    v->u.o.size = 0;
    v->u.o.capacity = capacity;
    v->u.o.m = capacity > 0 ? (lept_member*)malloc(capacity * sizeof(lept_member)) : NULL;
}

size_t lept_get_object_size(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    return v->u.o.size;
}

size_t lept_get_object_capacity(const lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    /* \todo */
    return 0;
}

void lept_reserve_object(lept_value* v, size_t capacity) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    /* \todo */
}

void lept_shrink_object(lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    /* \todo */
}

void lept_clear_object(lept_value* v) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    /* \todo */
}

const char* lept_get_object_key(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].k;
}

size_t lept_get_object_key_length(const lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    return v->u.o.m[index].klen;
}

lept_value* lept_get_object_value(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT);
    assert(index < v->u.o.size);
    return &v->u.o.m[index].v;
}

//我们许多时候需要查询一个键值是否存在，如存在，要获得其相应的值。
size_t lept_find_object_index(const lept_value* v, const char* key, size_t klen) {
    size_t i;
    assert(v != NULL && v->type == LEPT_OBJECT && key != NULL);
    for (i = 0; i < v->u.o.size; i++)
        if (v->u.o.m[i].klen == klen && memcmp(v->u.o.m[i].k, key, klen) == 0)
            return i;
    return LEPT_KEY_NOT_EXIST;
}
lept_value* lept_find_object_value(lept_value* v, const char* key, size_t klen) {
    size_t index = lept_find_object_index(v, key, klen);
    return index != LEPT_KEY_NOT_EXIST ? &v->u.o.m[index].v : NULL;
}

//为对象设置键值
lept_value* lept_set_object_value(lept_value* v, const char* key, size_t klen) {
    assert(v != NULL && v->type == LEPT_OBJECT && key != NULL);
    /* \todo */
    return NULL;
}
// 凡涉及赋值，都可能会引起资源拥有权问题。
// 值 s 并不能以指针方式简单地写入对象 v，因为这样便会有两个地方都拥有 s，会做成重复释放的 bug。
// 我们有两个选择：
//在 lept_set_object_value() 中，把参数 value 深度复制一个值，即把整个树复制一份，写入其新增的键值对中。
//在 lept_set_object_value() 中，把参数 value 拥有权转移至新增的键值对，再把 value 设置成 null 值。这就是所谓的移动语意。


void lept_remove_object_value(lept_value* v, size_t index) {
    assert(v != NULL && v->type == LEPT_OBJECT && index < v->u.o.size);
    /* \todo */
}
