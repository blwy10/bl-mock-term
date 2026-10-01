// mtsrv_fields.h                                                     -*-C++-*-
#ifndef INCLUDED_MTSRV_FIELDS
#define INCLUDED_MTSRV_FIELDS

//@PURPOSE: Provide the field dictionary and field-value extraction.
//
//@CLASSES:
//  mtsrv::FieldCategory:    real-time vs. static fields
//  mtsrv::FieldDef:         one dictionary entry
//  mtsrv::FieldDictionary:  lookup over the built-in field table
//  mtsrv::FieldMapper:      populate 'mtcore::Element's from data
//
//@DESCRIPTION: Clients request data by field mnemonic (e.g. "LAST_PRICE").
// The dictionary is served by the "//mt/refdata" 'FieldList' operation.

#include <mtcore_element.h>

#include <string>
#include <vector>

namespace mtsrv {

struct Security;
struct Tick;

struct FieldCategory {
    enum Value {
        e_REALTIME = 0,
        e_STATIC   = 1
    };

    static const char *toString(Value value);
};

struct FieldDef {
    const char              *mnemonic;
    const char              *fieldId;
    mtcore::DataType::Value  type;
    FieldCategory::Value     category;
    const char              *description;
};

struct FieldDictionary {
    static int count();
    static const FieldDef& at(int index);
    static const FieldDef *lookup(const std::string& mnemonic);
        // Case-insensitive.  Return NULL if unknown.

    static void realtimeFields(std::vector<const FieldDef *> *result);
    static void staticFields(std::vector<const FieldDef *> *result);

    static void resolve(std::vector<const FieldDef *> *result,
                        std::vector<std::string>      *invalid,
                        const mtcore::Element         *fieldArray,
                        bool                           defaultRealtime);
        // Resolve the mnemonics in 'fieldArray' (an array of strings; may be
        // NULL).  Unknown mnemonics are appended to 'invalid'.  If no fields
        // are named, load the real-time fields (if 'defaultRealtime') or all
        // fields.
};

struct FieldMapper {
    static bool populate(mtcore::Element *parent,
                         const FieldDef&  field,
                         const Security&  security,
                         const Tick      *tick);
        // Add a child named 'field.mnemonic' to 'parent' with the value of
        // 'field' for 'security'.  Real-time fields need 'tick'.  Return
        // 'false' if the field has no value (e.g. fundamentals on an FX
        // pair).

    static void populateAll(mtcore::Element                      *parent,
                            const std::vector<const FieldDef *>&  fields,
                            const Security&                       security,
                            const Tick                           *tick);
};

}  // close namespace mtsrv

#endif
