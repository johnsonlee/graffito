#ifndef TEXT_H
#define TEXT_H

#define _strltrim_(begin, end) ({                           \
    while (begin < end && isspace((int) *begin)) begin++;   \
})

#define strltrim(str) ({                                    \
    __typeof__(str) _p0_ = (str);                           \
    __typeof__(str) _p1_ = _p0_ + strlen(_p0_);             \
    _strltrim_(_p0_, _p1_);                                 \
    _p0_;                                                   \
})

#define _strrtrim_(begin, end) ({                           \
    while (begin < end && isspace((int) *(end - 1))) end--; \
    *end = '\0';                                            \
})

#define strrtrim(str) ({                                    \
    __typeof__(str) _p0_ = (str);                           \
    __typeof__(str) _p1_ = _p0_ + strlen(_p0_);             \
    _strrtrim_(_p0_, _p1_);                                 \
    (str);                                                  \
})

#define strtrim(str) ({                                     \
    __typeof__(str) _p0_ = (str);                           \
    __typeof__(str) _p1_ = _p0_ + strlen(_p0_);             \
    _strltrim_(_p0_, _p1_);                                 \
    _strrtrim_(_p0_, _p1_);                                 \
    _p0_;                                                   \
})

#endif /* TEXT_H */
