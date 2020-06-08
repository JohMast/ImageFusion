/*
 * The Lean Mean C++ Option Parser (modified)
 *
 * Copyright (C) 2012 Matthias S. Benkmann
 * Copyright (C) 2016 Christof Kaufmann
 *
 * The "Software" in the following 2 paragraphs refers to this file containing
 * the code to The Lean Mean C++ Option Parser.
 * The "Software" does NOT refer to any other files which you
 * may have received alongside this file (e.g. as part of a larger project that
 * incorporates The Lean Mean C++ Option Parser).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software, to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#pragma once

#include <string>
#include <vector>
#include <iostream>

#include "exceptions.h"

/**
 * @file
 *
 * @brief These are the more internal things of imagefusion::option.
 *
 * The parsing and pretty printing originates from 'The Lean Mean C++ Option Parser',
 * http://optionparser.sourceforge.net/index.html. Checking, storage and access has been modified.
 * Parsing and some other features have been added.
 *
 */

namespace imagefusion {
namespace option {

struct Descriptor;
struct IStringWriter;


#ifdef _MSC_VER
#include <intrin.h>
#pragma intrinsic(_BitScanReverse)
struct MSC_Builtin_CLZ
{
  static int builtin_clz(unsigned x)
  {
    unsigned long index;
    _BitScanReverse(&index, x);
    return 32-index; // int is always 32bit on Windows, even for target x64
  }
};
#define __builtin_clz(x) MSC_Builtin_CLZ::builtin_clz(x)
#endif


}
}


/** @brief The namespace of The Lean Mean C++ Option Parser. */
namespace imagefusion {
namespace option_impl_detail {


/**
 * @internal
 * @brief Compare an long option name to an argument from command line
 *
 * @param longname is the long option name as given in the Descriptor.
 *
 * @param arg is the argument given on commandline
 *
 * @param min is the minimum length that has to match.
 *
 * @par When `min = 0`:
 *
 * @returns true iff @c longname is a prefix of @c arg and in case @c arg is longer than @c
 * longname, then the first additional character is '='.
 *
 * @par Examples:
 * @code
 * streq("foo",     "foo=bar") == true
 * streq("foo",     "foobar")  == false
 * streq("foo",     "foo")     == true
 * streq("foo=bar", "foo")     == false
 * @endcode
 *
 *
 * @par When `min > 0`:
 *
 * @returns true iff @c longname and @c arg have a common prefix with the following properties:
 *  * its length is at least @c min characters or the same length as @c longname (whichever is
 *    smaller).
 *  * within @c arg the character following the common prefix is either '=' or end-of-string.
 *
 * Examples:
 * @code
 * streq("foo", "foo=bar", <anything>) == true
 * streq("foo", "fo=bar",   2)         == true
 * streq("foo", "fo",       2)         == true
 * streq("foo", "fo",       0)         == false
 * streq("foo", "f=bar",    2)         == false
 * streq("foo", "f",        2)         == false
 * streq("fo",  "foo=bar", <anything>) == false
 * streq("foo", "foobar",  <anything>) == false
 * streq("foo", "fobar",   <anything>) == false
 * streq("foo", "foo",     <anything>) == true
 * @endcode
 */
inline bool streq(std::string const& longname, std::string const& arg, unsigned int min = 0)
{
    if (min == 0 || min > longname.size())
        min = longname.size();
    if (arg.size() < min)
        return false;
    auto its = std::mismatch(std::begin(longname), std::end(longname),
                             std::begin(arg));
    return its.first - std::begin(longname) >= min &&
          (its.second == std::end(arg) || *its.second == '=');
}


/**
 * @internal
 * @brief Sets <code> i1 = max(i1, i2) </code>
 */
inline void upmax(int& i1, int i2)
{
    i1 = (i1 >= i2 ? i1 : i2);
}

/**
 * @internal
 * @brief Moves the "cursor" to column @c want_x assuming it is currently at column @c x and sets
 * @c x=want_x . If <code> x > want_x </code>, a line break is output before indenting.
 *
 * @param write Spaces and possibly a line break are written via this functor to get the desired
 * indentation @c want_x .
 *
 * @param[in,out] x the current indentation. Set to @c want_x by this method.
 * @param want_x the desired indentation.
 */
void indent(imagefusion::option::IStringWriter& write, int& x, int want_x);

/**
 * @brief Returns true if ch is the unicode code point of a wide character.
 *
 * @note
 * The following character ranges are treated as wide
 * @code
 * 1100..115F
 * 2329..232A  (just 2 characters!)
 * 2E80..A4C6  except for 303F
 * A960..A97C
 * AC00..D7FB
 * F900..FAFF
 * FE10..FE6B
 * FF01..FF60
 * FFE0..FFE6
 * 1B000......
 * @endcode
 */
inline bool isWideChar(unsigned ch)
{
    if (ch == 0x303F)
        return false;

    return ((0x1100 <= ch && ch <= 0x115F) || (0x2329 <= ch && ch <= 0x232A) || (0x2E80 <= ch && ch <= 0xA4C6)
            || (0xA960 <= ch && ch <= 0xA97C) || (0xAC00 <= ch && ch <= 0xD7FB) || (0xF900 <= ch && ch <= 0xFAFF)
            || (0xFE10 <= ch && ch <= 0xFE6B) || (0xFF01 <= ch && ch <= 0xFF60) || (0xFFE0 <= ch && ch <= 0xFFE6)
            || (0x1B000 <= ch));
}

/**
 * @internal
 * @brief Splits a @c Descriptor[] array into tables, rows, lines and columns and iterates over
 * these components.
 *
 * The top-level organizational unit is the @e table. A table begins at a Descriptor with @c
 * help!=NULL and extends up to a Descriptor with @c help==NULL.
 *
 * A table consists of @e rows. Due to line-wrapping and explicit breaks a row may take multiple
 * lines on screen. Rows within the table are separated by \\n. They never cross Descriptor
 * boundaries. This means a row ends either at \\n or the 0 at the end of the help string.
 *
 * A row consists of columns/cells. Columns/cells within a row are separated by \\t. Line breaks
 * within a cell are marked by \\v.
 *
 * Rows in the same table need not have the same number of columns/cells. The extreme case are
 * interjections, which are rows that contain neither \\t nor \\v. These are NOT treated specially
 * by LinePartIterator, but they are treated specially by printUsage().
 *
 * LinePartIterator iterates through the usage at 3 levels: table, row and part. Tables and rows
 * are as described above. A @e part is a line within a cell. LinePartIterator iterates through 1st
 * parts of all cells, then through the 2nd parts of all cells (if any),... @n
 * Example: The row <code> "1 \v 3 \t 2 \v 4" </code> has 2 cells/columns and 4 parts. The parts
 * will be returned in the order 1, 2, 3, 4.
 *
 * It is possible that some cells have fewer parts than others. In this case LinePartIterator will
 * "fill up" these cells with 0-length parts. IOW, LinePartIterator always returns the same number
 * of parts for each column. Note that this is different from the way rows and columns are handled.
 * LinePartIterator does @e not guarantee that the same number of columns will be returned for each
 * row.
 */
class LinePartIterator
{
    using descr_it_type = std::vector<imagefusion::option::Descriptor>::const_iterator;
    descr_it_type tablestart; //!< The 1st descriptor of the current table.
    descr_it_type rowdesc; //!< The Descriptor that contains the current row.
    descr_it_type const usage_end; //!< The Descriptor that contains the current row.
    const char* rowstart; //!< Ptr to 1st character of current row within rowdesc->help.
    const char* ptr; //!< Ptr to current part within the current row.
    int col; //!< Index of current column.
    int len; //!< Length of the current part (that ptr points at) in BYTES
    int screenlen; //!< Length of the current part in screen columns (taking narrow/wide chars into account).
    int max_line_in_block; //!< Greatest index of a line within the block. This is the number of \\v within the cell with the most \\vs.
    int line_in_block; //!< Line index within the current cell of the current part.
    int target_line_in_block; //!< Line index of the parts we should return to the user on this iteration.
    bool hit_target_line; //!< Flag whether we encountered a part with line index target_line_in_block in the current cell.
    int current_row; //!< Current row counter

    /**
     * @brief Determines the byte and character lengths of the part at @ref ptr and stores them in
     * @ref len and @ref screenlen respectively.
     */
    void update_length();

public:
    //! @brief Creates an iterator for @c usage.
    LinePartIterator(std::vector<imagefusion::option::Descriptor> const& usage) :
        tablestart(std::begin(usage)), rowdesc(std::end(usage)), usage_end(std::end(usage)), rowstart(nullptr), ptr(nullptr), col(-1), len(0), max_line_in_block(0), line_in_block(0),
        target_line_in_block(0), hit_target_line(true), current_row(0)
    {
    }

    /**
     * @brief Moves iteration to the next table (if any). Has to be called once on a new
     * LinePartIterator to move to the 1st table.
     * @retval false if moving to next table failed because no further table exists.
     */
    bool nextTable();

    /**
     * @brief Reset iteration to the beginning of the current table.
     */
    void restartTable();

    /**
     * @brief Moves iteration to the next row (if any). Has to be called once after each call to
     * @ref nextTable() to move to the 1st row of the table.
     * @retval false if moving to next row failed because no further row exists.
     */
    bool nextRow();

    /**
     * @brief Reset iteration to the beginning of the current row.
     */
    void restartRow()
    {
        ptr = rowstart;
        col = -1;
        len = 0;
        screenlen = 0;
        max_line_in_block = 0;
        line_in_block = 0;
        target_line_in_block = 0;
        hit_target_line = true;
    }

    /**
     * @brief Moves iteration to the next part (if any). Has to be called once after each call to
     * @ref nextRow() to move to the 1st part of the row.
     * @retval false if moving to next part failed because no further part exists.
     *
     * See @ref LinePartIterator for details about the iteration.
     */
    bool next();

    /**
     * @brief Returns the index (counting from 0) of the column in which the part pointed to by
     * @ref data() is located.
     */
    int column()
    {
        return col;
    }

    /**
     * @brief Returns the index (counting from 0) of the line within the current column this part
     * belongs to.
     */
    int line()
    {
        return target_line_in_block; // NOT line_in_block !!! It would be wrong if !hit_target_line
    }

    /**
     * @brief Returns the current row of the table.
     */
    int row()
    {
        return current_row + line_in_block;
    }

    /**
     * @brief Returns the length of the part pointed to by @ref data() in raw chars (not UTF-8
     * characters).
     */
    int length()
    {
        return len;
    }

    /**
     * @brief Returns the width in screen columns of the part pointed to by @ref data(). Takes
     * multi-byte UTF-8 sequences and wide characters into account.
     */
    int screenLength()
    {
        return screenlen;
    }

    /**
     * @brief Returns the current part of the iteration.
     */
    const char* data()
    {
        return ptr;
    }
};

/**
 * @internal
 * @brief Takes input and line wraps it, writing out one line at a time so that it can be
 * interleaved with output from other columns.
 *
 * The LineWrapper is used to handle the last column of each table as well as interjections. The
 * LineWrapper is called once for each line of output. If the data given to it fits into the
 * designated width of the last column it is simply written out. If there is too much data, an
 * appropriate split point is located and only the data up to this split point is written out. The
 * rest of the data is queued for the next line. That way the last column can be line wrapped and
 * interleaved with data from other columns. The following example makes this clearer:
 * @code
 * Column 1,1    Column 2,1     This is a long text
 * Column 1,2    Column 2,2     that does not fit into
 *                              a single line.
 * @endcode
 *
 * The difficulty in producing this output is that the whole string "This is a long text that does
 * not fit into a single line" is the 1st and only part of column 3. In order to produce the above
 * output the string must be output piecemeal, interleaved with the data from the other columns.
 */
class LineWrapper
{
    static const int bufmask = 15; //!< Must be a power of 2 minus 1.
    /**
     * @brief Ring buffer for length component of pair (data, length).
     */
    int lenbuf[bufmask + 1];
    /**
     * @brief Ring buffer for data component of pair (data, length).
     */
    const char* datbuf[bufmask + 1];
    /**
     * @brief The indentation of the column to which the LineBuffer outputs. LineBuffer assumes
     * that the indentation has already been written when @ref process() is called, so this value
     * is only used when a buffer flush requires writing additional lines of output.
     */
    int x;
    /**
     * @brief The width of the column to line wrap.
     */
    int width;
    int head; //!< @brief index for next write
    int tail; //!< @brief index for next read - 1 (i.e. increment tail BEFORE read)

    /**
     * @brief Multiple methods of LineWrapper may decide to flush part of the buffer to free up
     * space. The contract of process() says that only 1 line is output. So this variable is used
     * to track whether something has output a line. It is reset at the beginning of process() and
     * checked at the end to decide if output has already occurred or is still needed.
     */
    bool wrote_something;

    bool buf_full()
    {
        return tail == head;
    }

    void buf_store(const char* data, int len)
    {
        lenbuf[head] = len;
        datbuf[head] = data;
        head = (head + 1) & bufmask;
    }

    //! @brief Call BEFORE reading ...buf[tail].
    void buf_next()
    {
        tail = (tail + 1) & bufmask;
    }

    /**
     * @brief Writes (data,len) into the ring buffer. If the buffer is full, a single line is
     * flushed out of the buffer into @c write.
     */
    void output(imagefusion::option::IStringWriter& write, const char* data, int len);

    /**
     * @brief Writes a single line of output from the buffer to @c write.
     */
    void write_one_line(imagefusion::option::IStringWriter& write);

public:

    bool buf_empty()
    {
        return ((tail + 1) & bufmask) == head;
    }

    /**
     * @brief Writes out all remaining data from the LineWrapper using @c write. Unlike @ref
     * process() this method indents all lines including the first and will output a \\n at the end
     * (but only if something has been written).
     */
    void flush(imagefusion::option::IStringWriter& write);

    /**
     * @brief Process, wrap and output the next piece of data.
     *
     * process() will output at least one line of output. This is not necessarily the @c data
     * passed in. It may be data queued from a prior call to process(). If the internal buffer is
     * full, more than 1 line will be output.
     *
     * process() assumes that the a proper amount of indentation has already been output. It won't
     * write any further indentation before the 1st line. If more than 1 line is written due to
     * buffer constraints, the lines following the first will be indented by this method, though.
     *
     * No \\n is written by this method after the last line that is written.
     *
     * @param write where to write the data.
     * @param data the new chunk of data to write.
     * @param len the length of the chunk of data to write.
     */
    void process(imagefusion::option::IStringWriter& write, const char* data, int len);

    /**
     * @brief Constructs a LineWrapper that wraps its output to fit into screen columns @c x1
     * (incl.) to @c x2 (excl.).
     *
     * @c x1 gives the indentation LineWrapper uses if it needs to indent.
     */
    LineWrapper(int x1, int x2) :
        x(x1), width(x2 - x1), head(0), tail(bufmask)
    {
        if (width < 2) // because of wide characters we need at least width 2 or the code breaks
            width = 2;
    }

    LineWrapper() { }
};

} /* namespace option_impl_detail */
} /* namespace imagefusion */
