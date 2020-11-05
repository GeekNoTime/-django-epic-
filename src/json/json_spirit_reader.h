#ifndef JSON_SPIRIT_READER
#define JSON_SPIRIT_READER

//          Copyright John W. Wilkinson 2007 - 2009.
// Distributed under the MIT License, see accompanying file LICENSE.txt

// json spirit version 4.03

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
# pragma once
#endif

#include "json_spirit_value.h"
#include "json_spirit_error_position.h"
#include <iostream>

namespace json_spirit
{
    // functions to reads a JSON values

    bool read( const std::string& s, Value& value );
    bool read( std::istream& is,     Value& value );
    bool read( std::string::const_iterator& begin, std::string::const_iterator end, Value& value );

    void read_or_throw( const std::string& s, Value& value );  
    void read_or_throw( std::istream& is,     Value& value );
    void read_or_throw( std::string::const_iterator& begin, std::string::const_iterator end, Value& value );

#ifndef BOOST_NO_STD_WSTRING

    bool read( const std::wstring& s, wValue& value );
