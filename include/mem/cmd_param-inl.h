/*
    Copyright 2018 Brick

    Permission is hereby granted, free of charge, to any person obtaining a copy of this software
    and associated documentation files (the "Software"), to deal in the Software without restriction,
    including without limitation the rights to use, copy, modify, merge, publish, distribute,
    sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all copies or
    substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
    BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
    DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#if defined(MEM_CMD_PARAM_INL_BRICK_H)
# error mem/cmd_param-inl.h should only be included once
#endif // MEM_CMD_PARAM_INL_BRICK_H

#define MEM_CMD_PARAM_INL_BRICK_H

#include "cmd_param.h"

#include <cctype>
#include <cstring>

namespace mem
{
    cmd_param* cmd_param::ROOT {nullptr};

    inline bool cmd_is_digit(int c)
    {
        return (static_cast<unsigned int>(c) - '0') < 10;
    }

    inline int cmd_to_lower(int c)
    {
        if ((static_cast<unsigned int>(c) - 'A') < 26)
        {
            c += ('a' - 'A');
        }

        return c;
    }

    int cmd_strcmp(const char* lhs, const char* rhs)
    {
        int a, b;

        do
        {
            a = cmd_to_lower(static_cast<unsigned char>(*lhs++));
            b = cmd_to_lower(static_cast<unsigned char>(*rhs++));
        } while (a && a == b);

        if (a == '\0' && b == '=')
            b = '\0';

        return a - b;
    }

    inline bool cmd_is_option(const char* arg)
    {
        return (arg[0] == '-') && !cmd_is_digit(static_cast<unsigned char>(arg[1]));
    }

    inline char* cmd_unquote(char* arg)
    {
        if (arg[0] == '"')
        {
            ++arg;

            char* last = arg + std::strlen(arg) - 1;

            if (last[0] == '"')
                last[0] = '\0';
        }

        return arg;
    }

    void cmd_param::init(char** argv)
    {
        int argc = 0;

        while (argv[argc])
        {
            ++argc;
        }

        init(argc, argv);
    }

    void cmd_param::init(int argc, char** argv)
    {
        bool done_positionals = false;

        for (int i = 1; i < argc; ++i)
        {
            char* arg = argv[i];

            if (cmd_is_option(arg))
            {
                while (arg[0] == '-')
                    ++arg;

                done_positionals = true;

                bool used = false;

                for (cmd_param* j = ROOT; j; j = j->next_)
                {
                    if (j->name_)
                    {
                        if (!cmd_strcmp(j->name_, arg))
                        {
                            const char* value = "";

                            if (char* val = std::strchr(arg, '='))
                            {
                                value = cmd_unquote(val + 1);
                            }
                            else if (i + 1 < argc)
                            {
                                char* next_arg = argv[i + 1];

                                if (!cmd_is_option(next_arg))
                                {
                                    value = cmd_unquote(next_arg);
                                }
                            }

                            j->value_ = value;

                            used = true;
                        }
                    }
                }

                if (!used)
                {
                    for (cmd_param* j = ROOT; j; j = j->next_)
                    {
                        if (j->name_)
                        {
                            if ((!std::strncmp(arg,      "no", 2) && !cmd_strcmp(j->name_, arg + 2)) ||
                                (!std::strncmp(j->name_, "no", 2) && !cmd_strcmp(j->name_ + 2, arg)))
                            {
                                j->value_ = "false";

                                used = true;
                            }
                        }
                    }
                }
            }
            else if (!done_positionals)
            {
                for (cmd_param* j = ROOT; j; j = j->next_)
                {
                    if (j->pos_ == i)
                    {
                        const char* value = cmd_unquote(arg);

                        j->value_ = value;
                    }
                }
            }
        }
    }
}
