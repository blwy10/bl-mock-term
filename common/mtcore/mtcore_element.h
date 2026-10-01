// mtcore_element.h                                                   -*-C++-*-
#ifndef INCLUDED_MTCORE_ELEMENT
#define INCLUDED_MTCORE_ELEMENT

//@PURPOSE: Provide a self-describing tree of named, typed values.
//
//@CLASSES:
//  mtcore::DataType:  enumeration of scalar types
//  mtcore::Element:   named node holding a scalar, an array or a sequence
//
//@DESCRIPTION: 'Element' is the payload model for every request, response
// and subscription update on the wire, in the style of the request/response
// "elements" of market-data APIs.  A node is one of:
//: o a scalar ('BOOL', 'INT64', 'FLOAT64', 'STRING', 'DATETIME'),
//: o a sequence (named children, looked up by name), or
//: o an array (unnamed items appended in order).
//
// Children are owned through raw pointers and deep-copied on copy.
//
//@USAGE:
//..
//  mtcore::Element body("request");
//  body.setChildString("security", "AAPL US Equity");
//  mtcore::Element *fields = body.addChild("fields");
//  fields->setArray(true);
//  fields->appendItem()->setString("LAST_PRICE");
//..

#include "mtcore_platform.h"

#include <ostream>
#include <string>
#include <vector>

namespace mtcore {

struct DataType {
    enum Value {
        e_NONE     = 0,
        e_BOOL     = 1,
        e_INT64    = 2,
        e_FLOAT64  = 3,
        e_STRING   = 4,
        e_DATETIME = 5,   // epoch milliseconds
        e_SEQUENCE = 6,
        e_ARRAY    = 7
    };

    static const char *toString(Value value);
};

class Element {
    typedef std::vector<Element *> ChildList;

    std::string     d_name;
    DataType::Value d_type;
    union {
        bool   d_bool;
        Int64  d_int;
        double d_double;
    }               d_value;
    std::string     d_string;    // used when 'd_type == e_STRING'
    ChildList       d_children;  // owned

  private:
    void copyFrom(const Element& other);
    void destroyChildren();

  public:
    Element();
    explicit Element(const std::string& name);
    Element(const Element& other);
    ~Element();
    Element& operator=(const Element& rhs);

    // MANIPULATORS
    void setName(const std::string& name);
    void clear();
        // Reset to 'e_NONE' and delete all children.  The name is kept.

    void setBool(bool value);
    void setInt64(Int64 value);
    void setFloat64(double value);
    void setString(const std::string& value);
    void setDatetime(Int64 epochMillis);

    void setArray(bool isArray);
        // Make this node an empty array (or empty sequence if 'false').

    Element *addChild(const std::string& name);
        // Return the child named 'name', creating it if necessary.  This
        // node becomes a sequence.
    Element *appendItem();
        // Append an unnamed item.  This node becomes an array.
    Element *getChild(const std::string& name);
    bool removeChild(const std::string& name);

    void setChildBool(const std::string& name, bool value);
    void setChildInt64(const std::string& name, Int64 value);
    void setChildFloat64(const std::string& name, double value);
    void setChildString(const std::string& name, const std::string& value);
    void setChildDatetime(const std::string& name, Int64 epochMillis);

    void merge(const Element& other);
        // Overwrite/insert every child of 'other' into this sequence.

    // ACCESSORS
    const std::string& name() const;
    DataType::Value type() const;
    bool isNull() const;
    bool isArray() const;
    bool isSequence() const;
    bool isScalar() const;

    bool getBool() const;
    Int64 getInt64() const;
        // Numeric conversion from 'FLOAT64' and 'BOOL' is performed.
    double getFloat64() const;
    const std::string& getString() const;
        // Return an empty string for non-string nodes.
    std::string toString() const;
        // Text rendering of a scalar value.

    size_t numChildren() const;
    const Element *childAt(size_t index) const;
    Element *childAt(size_t index);
    const Element *getChild(const std::string& name) const;
    bool hasChild(const std::string& name) const;

    std::string getChildString(const std::string& name,
                               const std::string& defaultValue = "") const;
    Int64 getChildInt64(const std::string& name, Int64 defaultValue = 0) const;
    double getChildFloat64(const std::string& name,
                           double             defaultValue = 0) const;
    bool getChildBool(const std::string& name, bool defaultValue = false) const;

    void print(std::ostream& stream, int level = 0) const;
        // Human-readable, indented rendering (for logs and 'mtcli').

    bool equals(const Element& other) const;
};

std::ostream& operator<<(std::ostream& stream, const Element& element);

}  // close namespace mtcore

#endif
