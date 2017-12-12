#include <gtest/gtest.h>
#include <pugixml.hpp>
#include <iostream>
#include <libsk.h>

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
    ASSERT_STREQ(log_config.name(), "log_config");

    pugi::xml_node name = log_config.child("name");
    ASSERT_TRUE(name);
    ASSERT_STREQ(name.name(), "name");
    ASSERT_STREQ(name.value(), "");
    ASSERT_STREQ(name.child_value(), "world");
    ASSERT_STREQ(name.text().as_string(), "world");
}
