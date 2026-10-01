// mtcore_element.cpp                                                 -*-C++-*-
#include "mtcore_element.h"

#include "mtcore_stringutil.h"

#include <stdio.h>

namespace mtcore {

const char *DataType::toString(Value value)
{
    switch (value) {
      case e_NONE:     return "NONE";
      case e_BOOL:     return "BOOL";
      case e_INT64:    return "INT64";
      case e_FLOAT64:  return "FLOAT64";
      case e_STRING:   return "STRING";
      case e_DATETIME: return "DATETIME";
      case e_SEQUENCE: return "SEQUENCE";
      case e_ARRAY:    return "ARRAY";
    }
    return "???";
}

                                // -------------
                                // class Element
                                // -------------

Element::Element()
: d_type(DataType::e_NONE)
{
    d_value.d_int = 0;
}

Element::Element(const std::string& name)
: d_name(name)
, d_type(DataType::e_NONE)
{
    d_value.d_int = 0;
}

Element::Element(const Element& other)
: d_type(DataType::e_NONE)
{
    d_value.d_int = 0;
    copyFrom(other);
}

Element::~Element()
{
    destroyChildren();
}

Element& Element::operator=(const Element& rhs)
{
    if (this != &rhs) {
        destroyChildren();
        copyFrom(rhs);
    }
    return *this;
}

void Element::copyFrom(const Element& other)
{
    d_name   = other.d_name;
    d_type   = other.d_type;
    d_value  = other.d_value;
    d_string = other.d_string;
    d_children.reserve(other.d_children.size());
    for (ChildList::const_iterator it = other.d_children.begin();
         it != other.d_children.end();
         ++it) {
        d_children.push_back(new Element(**it));
    }
}

void Element::destroyChildren()
{
    for (ChildList::iterator it = d_children.begin();
         it != d_children.end();
         ++it) {
        delete *it;
    }
    d_children.clear();
}

void Element::setName(const std::string& name)
{
    d_name = name;
}

void Element::clear()
{
    destroyChildren();
    d_type        = DataType::e_NONE;
    d_value.d_int = 0;
    d_string.clear();
}

void Element::setBool(bool value)
{
    clear();
    d_type         = DataType::e_BOOL;
    d_value.d_bool = value;
}

void Element::setInt64(Int64 value)
{
    clear();
    d_type        = DataType::e_INT64;
    d_value.d_int = value;
}

void Element::setFloat64(double value)
{
    clear();
    d_type           = DataType::e_FLOAT64;
    d_value.d_double = value;
}

void Element::setString(const std::string& value)
{
    clear();
    d_type   = DataType::e_STRING;
    d_string = value;
}

void Element::setDatetime(Int64 epochMillis)
{
    clear();
    d_type        = DataType::e_DATETIME;
    d_value.d_int = epochMillis;
}

void Element::setArray(bool isArray)
{
    clear();
    d_type = isArray ? DataType::e_ARRAY : DataType::e_SEQUENCE;
}

Element *Element::addChild(const std::string& name)
{
    if (d_type != DataType::e_SEQUENCE) {
        clear();
        d_type = DataType::e_SEQUENCE;
    }
    Element *existing = getChild(name);
    if (existing) {
        return existing;
    }
    Element *child = new Element(name);
    d_children.push_back(child);
    return child;
}

Element *Element::appendItem()
{
    if (d_type != DataType::e_ARRAY) {
        clear();
        d_type = DataType::e_ARRAY;
    }
    Element *child = new Element();
    d_children.push_back(child);
    return child;
}

Element *Element::getChild(const std::string& name)
{
    for (ChildList::iterator it = d_children.begin();
         it != d_children.end();
         ++it) {
        if ((*it)->d_name == name) {
            return *it;
        }
    }
    return NULL;
}

bool Element::removeChild(const std::string& name)
{
    for (ChildList::iterator it = d_children.begin();
         it != d_children.end();
         ++it) {
        if ((*it)->d_name == name) {
            delete *it;
            d_children.erase(it);
            return true;
        }
    }
    return false;
}

void Element::setChildBool(const std::string& name, bool value)
{
    addChild(name)->setBool(value);
}

void Element::setChildInt64(const std::string& name, Int64 value)
{
    addChild(name)->setInt64(value);
}

void Element::setChildFloat64(const std::string& name, double value)
{
    addChild(name)->setFloat64(value);
}

void Element::setChildString(const std::string& name,
                             const std::string& value)
{
    addChild(name)->setString(value);
}

void Element::setChildDatetime(const std::string& name, Int64 epochMillis)
{
    addChild(name)->setDatetime(epochMillis);
}

void Element::merge(const Element& other)
{
    if (!other.isSequence()) {
        *this = other;
        return;
    }
    for (ChildList::const_iterator it = other.d_children.begin();
         it != other.d_children.end();
         ++it) {
        Element *target = addChild((*it)->d_name);
        *target = **it;
    }
}

const std::string& Element::name() const
{
    return d_name;
}

DataType::Value Element::type() const
{
    return d_type;
}

bool Element::isNull() const
{
    return d_type == DataType::e_NONE;
}

bool Element::isArray() const
{
    return d_type == DataType::e_ARRAY;
}

bool Element::isSequence() const
{
    return d_type == DataType::e_SEQUENCE;
}

bool Element::isScalar() const
{
    return d_type != DataType::e_NONE
        && d_type != DataType::e_ARRAY
        && d_type != DataType::e_SEQUENCE;
}

bool Element::getBool() const
{
    switch (d_type) {
      case DataType::e_BOOL:     return d_value.d_bool;
      case DataType::e_INT64:
      case DataType::e_DATETIME: return d_value.d_int != 0;
      case DataType::e_FLOAT64:  return d_value.d_double != 0;
      case DataType::e_STRING:   return d_string == "true" || d_string == "1";
      default:                   return false;
    }
}

Int64 Element::getInt64() const
{
    switch (d_type) {
      case DataType::e_BOOL:     return d_value.d_bool ? 1 : 0;
      case DataType::e_INT64:
      case DataType::e_DATETIME: return d_value.d_int;
      case DataType::e_FLOAT64:  return (Int64)d_value.d_double;
      case DataType::e_STRING: {
        Int64 v = 0;
        return StringUtil::toInt64(&v, d_string.c_str()) == 0 ? v : 0;
      }
      default:                   return 0;
    }
}

double Element::getFloat64() const
{
    switch (d_type) {
      case DataType::e_BOOL:     return d_value.d_bool ? 1.0 : 0.0;
      case DataType::e_INT64:
      case DataType::e_DATETIME: return (double)d_value.d_int;
      case DataType::e_FLOAT64:  return d_value.d_double;
      case DataType::e_STRING: {
        double v = 0;
        return StringUtil::toDouble(&v, d_string.c_str()) == 0 ? v : 0;
      }
      default:                   return 0;
    }
}

const std::string& Element::getString() const
{
    static const std::string s_empty;
    return d_type == DataType::e_STRING ? d_string : s_empty;
}

std::string Element::toString() const
{
    char buf[64];
    switch (d_type) {
      case DataType::e_BOOL:
        return d_value.d_bool ? "true" : "false";
      case DataType::e_INT64:
        _snprintf(buf, sizeof(buf), "%I64d", d_value.d_int);
        return std::string(buf);
      case DataType::e_DATETIME:
        _snprintf(buf, sizeof(buf), "@%I64d", d_value.d_int);
        return std::string(buf);
      case DataType::e_FLOAT64:
        _snprintf(buf, sizeof(buf), "%.10g", d_value.d_double);
        return std::string(buf);
      case DataType::e_STRING:
        return d_string;
      default:
        return std::string();
    }
}

size_t Element::numChildren() const
{
    return d_children.size();
}

const Element *Element::childAt(size_t index) const
{
    return index < d_children.size() ? d_children[index] : NULL;
}

Element *Element::childAt(size_t index)
{
    return index < d_children.size() ? d_children[index] : NULL;
}

const Element *Element::getChild(const std::string& name) const
{
    for (ChildList::const_iterator it = d_children.begin();
         it != d_children.end();
         ++it) {
        if ((*it)->d_name == name) {
            return *it;
        }
    }
    return NULL;
}

bool Element::hasChild(const std::string& name) const
{
    return getChild(name) != NULL;
}

std::string Element::getChildString(const std::string& name,
                                    const std::string& defaultValue) const
{
    const Element *child = getChild(name);
    if (child == NULL || child->d_type != DataType::e_STRING) {
        return defaultValue;
    }
    return child->d_string;
}

Int64 Element::getChildInt64(const std::string& name,
                             Int64              defaultValue) const
{
    const Element *child = getChild(name);
    return child && child->isScalar() ? child->getInt64() : defaultValue;
}

double Element::getChildFloat64(const std::string& name,
                                double             defaultValue) const
{
    const Element *child = getChild(name);
    return child && child->isScalar() ? child->getFloat64() : defaultValue;
}

bool Element::getChildBool(const std::string& name, bool defaultValue) const
{
    const Element *child = getChild(name);
    return child && child->isScalar() ? child->getBool() : defaultValue;
}

void Element::print(std::ostream& stream, int level) const
{
    std::string indent(level * 2, ' ');
    stream << indent << (d_name.empty() ? "-" : d_name.c_str());
    if (isScalar()) {
        if (d_type == DataType::e_STRING) {
            stream << " = \"" << d_string << "\"\n";
        }
        else {
            stream << " = " << toString() << "\n";
        }
        return;
    }
    if (d_type == DataType::e_NONE) {
        stream << " = null\n";
        return;
    }
    stream << (isArray() ? " [\n" : " {\n");
    for (ChildList::const_iterator it = d_children.begin();
         it != d_children.end();
         ++it) {
        (*it)->print(stream, level + 1);
    }
    stream << indent << (isArray() ? "]\n" : "}\n");
}

bool Element::equals(const Element& other) const
{
    if (d_name != other.d_name || d_type != other.d_type) {
        return false;
    }
    switch (d_type) {
      case DataType::e_BOOL:
        return d_value.d_bool == other.d_value.d_bool;
      case DataType::e_INT64:
      case DataType::e_DATETIME:
        return d_value.d_int == other.d_value.d_int;
      case DataType::e_FLOAT64:
        return d_value.d_double == other.d_value.d_double;
      case DataType::e_STRING:
        return d_string == other.d_string;
      default:
        break;
    }
    if (d_children.size() != other.d_children.size()) {
        return false;
    }
    for (size_t i = 0; i < d_children.size(); ++i) {
        if (!d_children[i]->equals(*other.d_children[i])) {
            return false;
        }
    }
    return true;
}

std::ostream& operator<<(std::ostream& stream, const Element& element)
{
    element.print(stream, 0);
    return stream;
}

}  // close namespace mtcore
