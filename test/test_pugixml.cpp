#include <gtest/gtest.h>
#include <iostream>
#include "libsk.h"

TEST(pugixml, normal) {
    const char *xml = "          \
        <log_config>             \
          <name>world</name>     \
          <path>/app/log</path>  \
          <category>             \
            <type>file</type>    \
            <level>debug</level> \
          </category>            \
          <category>             \
            <type>console</type> \
            <level>info</level>  \
          </category>            \
        </log_config>            \
        ";
    pugi::xml_document doc;
    auto ok = doc.load_string(xml);
    ASSERT_TRUE(ok);
    ASSERT_STREQ(doc.name(), "");
    ASSERT_STREQ(doc.value(), "");
    ASSERT_TRUE(doc.type() == pugi::node_document);

    pugi::xml_node log_config = doc.child("log_config");
    ASSERT_TRUE(log_config);
    ASSERT_TRUE(log_config.name());
}
