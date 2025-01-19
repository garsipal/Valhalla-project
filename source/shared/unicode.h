// UTF-8 support

#include <string.h>

// reads the next character into `codepoint` and returns the number of bytes it takes up
// `str` must be null-terminated
static inline int uni_getchar(const char *str, unsigned int& codepoint)
{
    const unsigned char *p = (const unsigned char *)str;
    if(p[0] <= 0x7F)
    {
        codepoint = p[0];
        return 1;
    }
    if(p[0] >> 5 == 0x06 && p[1] >> 6 == 0x02)
    {
        codepoint = ((p[0] & 0x1F) << 6) | (p[1] & 0x3F);
        if(codepoint < 0x80) codepoint = 0xFFFD;
        return 2;
    }
    if(p[0] >> 4 == 0x0E && p[1] >> 6 == 0x02 && p[2] >> 6 == 0x02)
    {
        codepoint = ((p[0] & 0x0F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        if(codepoint < 0x800) codepoint = 0xFFFD;
        return 3;
    }
    if(p[0] >> 3 == 0x1E && p[1] >> 6 == 0x02 && p[2] >> 6 == 0x02 && p[3] >> 6 == 0x02)
    {
        codepoint = ((p[0] & 0x07) << 18) | ((p[1] & 0x3F) << 12) | ((p[2] & 0x3F) << 6) | (p[3] & 0x3F);
        if(codepoint < 0x10000 || codepoint > 0x10FFFF) codepoint = 0xFFFD;
        return 4;
    }
    codepoint = 0xFFFD;
    return 1;
}

// returns the number of bytes composing the previous char in the string
static inline int uni_prevchar(const char *str, int i)
{
    if(i <= 0) return 0;
    const unsigned char *p = (const unsigned char *)str;
    if(p[i-1] <= 0x7F) return 1;
    if(i >= 2 && p[i-2] >> 5 == 0x06 && p[i-1] >> 6 == 0x02) return 2;
    if(i >= 3 && p[i-3] >> 4 == 0x0E && p[i-2] >> 6 == 0x02 && p[i-1] >> 6 == 0x02) return 3;
    if(i >= 4 && p[i-4] >> 3 == 0x1E && p[i-3] >> 6 == 0x02 && p[i-2] >> 6 == 0x02 && p[i-1] >> 6 == 0x02) return 4;
    return 1;
}

// encodes a single codepoint
static inline int uni_code2str(unsigned int codepoint, char *dst)
{
    if(codepoint <= 0x007F)
    {
        dst[0] = codepoint;
        dst[1] = '\0';
        return 1;
    }
    if(codepoint <= 0x07FF)
    {
        dst[0] = 0xC0 | (codepoint >> 6);
        dst[1] = 0x80 | (codepoint & 0x3F);
        dst[2] = '\0';
        return 2;
    }
    if(codepoint <= 0xFFFF)
    {
        dst[0] = 0xE0 | (codepoint >> 12);
        dst[1] = 0x80 | ((codepoint >> 6) & 0x3F);
        dst[2] = 0x80 | (codepoint & 0x3F);
        dst[3] = '\0';
        return 3;
    }
    if(codepoint <= 0x10FFFF)
    {
        dst[0] = 0xF0 | (codepoint >> 18);
        dst[1] = 0x80 | ((codepoint >> 12) & 0x3F);
        dst[2] = 0x80 | ((codepoint >> 6) & 0x3F);
        dst[3] = 0x80 | (codepoint & 0x3F);
        dst[4] = '\0';
        return 4;
    }
    dst[0] = '\0';
    return 0;
}

static inline unsigned int uni_strlen(const char *str)
{
    unsigned int _c, len = 0;
    while(*str)
    {
        str += uni_getchar(str, _c);
        ++len;
    }
    return len;
}

// returns the codepoint of the character at index `ix`
static inline unsigned int uni_charat(const char *str, int ix)
{
    unsigned int c;
    if(ix >= 0)
    {
        for(int i = 0; i < ix; ++i)
        {
            if(!*str) return 0;
            str += uni_getchar(str, c);
        }
        if(!*str) return 0;
        uni_getchar(str, c);
        return c;
    }

    const unsigned int len = strlen(str);
    unsigned int b = 0;
    for(int i = 0; i > ix; --i)
    {
        b += uni_prevchar(str, len-b);
    }
    uni_getchar(str+len-b, c);
    return c;
}

// offset of the character at index `ix`
static inline int uni_offset(const char *str, unsigned int ix)
{
    const char *p = str;
    unsigned int _c;
    for(unsigned int i = 0; i < ix; ++i)
    {
        if(!*p) break;
        p += uni_getchar(p, _c);
    }
    return (p - str);
}
// same but starting from the end of the string (str[-ix])
static inline int uni_negoffset(const char *str, unsigned int ix)
{
    const char *p = str + strlen(str);
    for(unsigned int i = 0; i < ix; ++i)
    {
        if(p <= str) break;
        p -= uni_prevchar(str, p - str);
    }
    return (p - str);
}

// returns the index of the unicode character at offset `off`
static inline int uni_index(const char *str, unsigned int off)
{
    const char *p = str, *end = str + strlen(str);
    unsigned int _c;
    unsigned int ret = 0;
    while(ret < off)
    {
        if(p >= end || p >= str + off) return ret;
        p += uni_getchar(p, _c);
        ++ret;
    }
    return ret;
}

// only supports ASCII
static inline void uni_strlower(const char *src, char *dst)
{
    for(const unsigned char *p = (unsigned char *)src; *p; ++p)
    {
        if(*p >= 'A' && *p <= 'Z')
        {
            *dst = 'a' + (*p - 'A');
        }
        else *dst = *p;
        ++dst;
    }
}
static inline void uni_strupper(const char *src, char *dst)
{
    for(const unsigned char *p = (unsigned char *)src; *p; ++p)
    {
        if(*p >= 'a' && *p <= 'z')
        {
            *dst = 'A' + (*p - 'a');
        }
        else *dst = *p;
        ++dst;
    }
}