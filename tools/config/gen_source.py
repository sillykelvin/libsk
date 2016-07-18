#!/usr/bin/env python

import re
import sys
import string


def enum(**enums):
    return type('Enum', (), enums)

field_type = enum(
    s32    = 's32',
    u32    = 'u32',
    s64    = 's64',
    u64    = 'u64',
    string = 'string',
    vector = 'vector',
    custom = 'custom'
)

class ConfigDef:
    def __init__(self):
        self.name = None
        self.fields = []
        self.sub_def = []    # children definitions
        self.sup_def = None  # parent definition

    def __str__(self):
        return self.info(0)

    def info(self, indent):
        ret = '%sDEFINITION: %s\n%sfields:\n' % (' ' * indent, self.name, ' ' * indent)
        for f in self.fields:
            ret += f.info(indent + 4)

        for d in self.sub_def:
            ret += d.info(indent + 4)

        return ret

class FieldInfo:
    def __init__(self):
        self.field_type = None
        self.field_name = None
        # if the field is std::vector, sub_type is the type in vector
        # if the field is customized type, sub_type is that type
        self.sub_type   = None

    def __str__(self):
        return info(0)

    def info(self, indent):
        ret = '%stype: %s, name: %s' % (' ' * indent, self.field_type, self.field_name)
        if self.field_type in (field_type.vector, field_type.custom):
            assert self.sub_type
            ret += ', sub_type: %s' % (self.sub_type)
        ret += '\n'

        return ret

def build_config_def(root_lists, parent, name):
    _def = ConfigDef()
    _def.name = name
    if parent:
        _def.sup_def = parent
        parent.sub_def.append(_def)
    else:
        root_lists.append(_def)

    return _def

def build_field_info(line, type_str, name):
    field = FieldInfo()
    field.field_name = name

    if type_str == 'int' or type_str == 's32':
        field.field_type = field_type.s32
    elif type_str == 'u32':
        field.field_type = field_type.u32
    elif type_str == 's64':
        field.field_type = field_type.s64
    elif type_str == 'u64':
        field.field_type = field_type.u64
    elif type_str == 'std::string':
        field.field_type = field_type.string
    elif type_str.startswith('std::vector<'):
        field.field_type = field_type.vector
        ret = re.search('^std::vector< *([_a-zA-Z0-9]+) *>$', type_str)
        assert ret
        field.sub_type = ret.group(1)
    else:
        # customize types
        field.field_type = field_type.custom
        field.sub_type = type_str

    return field

def read_content(filename):
    content = None
    with open(filename, 'r') as f:
        content = f.read()

    content = content.replace('\r\n', '\n').replace('\t', '    ')

    # remove line comments // ... and block comments /* ... */
    content = re.sub('//.*\n', '\n', content)
    content = re.sub('/\*((?:.|\n)*?)\*/', '', content)

    # remove preprocessors
    content = re.sub('#ifndef.*\n', '\n', content)
    content = re.sub('#define.*\n', '\n', content)
    content = re.sub('#include.*\n', '\n', content)
    content = re.sub('#endif.*\n', '\n', content)

    # remove trailing whitespaces
    content = re.sub(' *\n', '\n', content)

    # make the brace after the definition, not the new line
    content = re.sub('\n *{', ' {\n', content)

    # remove extra empty lines
    content = re.sub('\n+', '\n', content)

    return content

def parse_content(content):
    def_list = []
    curr_def = None

    # split to lines, and remove empty ones
    lines = [x for x in content.split('\n') if x]

    for line in lines:
        line = line.strip()
        if not line: continue

        # the def start line
        ret = re.search('^DEF_CONFIG +([_a-zA-Z0-9]+) +{$', line)
        if ret:
            name = ret.group(1)
            _def = build_config_def(def_list, curr_def, name)
            curr_def = _def
            continue

        # the def end line
        ret = re.search('^ *};$', line)
        if ret:
            assert curr_def
            curr_def = curr_def.sup_def
            continue

        # the field line
        ret = re.search('^ *([_a-zA-Z0-9<>:]+) +([_a-zA-Z0-9]+);$', line)
        if ret:
            type_str = ret.group(1)
            name = ret.group(2)
            field = build_field_info(line, type_str, name)
            curr_def.fields.append(field)
            continue

        print "invalid line: %s" % (line)
        sys.exit(-1)

    return def_list

def write_string_field(field, conf_name):
    template = '''
    pugi::xml_node $field_name = $conf_name.child("$field_name");
    if (!$field_name) {
        fprintf(stderr, "node $field_name not found.", $field_name);
        return -EINVAL;
    }
    conf.$field_name = $field_name.text().get();
'''

    t = string.Template(template)
    return t.substitute(conf_name = conf_name, field_name = field.field_name)

def write_integral_field(field, conf_name):
    template = '''
    pugi::xml_node $field_name = $conf_name.child("$field_name");
    if (!$field_name) {
        fprintf(stderr, "node $field_name not found.", $field_name);
        return -EINVAL;
    }
    ret = sk::$func_name($field_name.text().get(), conf.$field_name);
    if (ret != 0) {
        fprintf(stderr, "cannot convert node $field_name.", $field_name);
        return ret;
    }
'''
    func = 'string2%s' % (field.field_type)
    t = string.Template(template)
    return t.substitute(conf_name = conf_name,
                        field_name = field.field_name,
                        func_name = func)

def write_vector_field(field, conf_name):
    template = '''
    auto ${field_name}_set = $conf_name.children("$field_name");
    for (auto it = ${field_name}_set.begin(), end = ${field_name}_set.end(); it != end; ++it) {
        const xml_node n = *it;
        $field_type obj;
        ret = load_node(obj, n);
        if (ret != 0)
            return ret;

        conf.$field_name.push_back(obj);
    }
    if (conf.$field_name.size() <= 0) {
        fprintf(stderr, "node $field_name not found.", $field_name);
        return -EINVAL;
    }
'''

    t = string.Template(template)
    return t.substitute(conf_name = conf_name,
                        field_name = field.field_name,
                        field_type = field.sub_type)

def write_custom_field(field, conf_name):
    template = '''
    auto ${field_name} = $conf_name.child("$field_name");
    if (!$field_name) {
        fprintf(stderr, "node $field_name not found.", $field_name);
        return -EINVAL;
    }
    ret = load_node(conf.$field_name, $field_name);
    if (ret != 0)
        return ret;
'''

    t = string.Template(template)
    return t.substitute(conf_name = conf_name,
                        field_name = field.field_name)

def write_source(def_list, filename):
    template = '''
int load_file($conf_name& conf, const char *file) {
    assert_retval(file, -1);

    int ret = 0;
    pugi::xml_document doc;
    auto ok = doc.load_file(file);
    if (!ok) {
        fprintf(stderr, "load file<%s> error: %s.", file, ok.description());
        return -EINVAL;
    }

    pugi::xml_node $conf_name = doc.child("$conf_name");
    if (!$conf_name) {
        fprintf(stderr, "node $conf_name not found.", $conf_name, file);
        return -EINVAL;
    }

    // read fields from xml
$fields_src

    return 0;
}
'''
    t = string.Template(template)

    with open(filename, 'w') as f:
        f.write('// Generated by tool, DO NOT EDIT!!!\n\n')

        for _def in def_list:
            fields_src = ''
            for field in _def.fields:
                if field.field_type == field_type.s32:
                    fields_src += write_integral_field(field, _def.name)
                elif field.field_type == field_type.u32:
                    fields_src += write_integral_field(field, _def.name)
                elif field.field_type == field_type.s64:
                    fields_src += write_integral_field(field, _def.name)
                elif field.field_type == field_type.u64:
                    fields_src += write_integral_field(field, _def.name)
                elif field.field_type == field_type.string:
                    fields_src += write_string_field(field, _def.name)
                elif field.field_type == field_type.vector:
                    fields_src += write_vector_field(field, _def.name)
                elif field.field_type == field_type.custom:
                    fields_src += write_custom_field(field, _def.name)

            f.write(t.substitute(conf_name = _def.name, fields_src = fields_src))


if __name__ == '__main__':
    argc = len(sys.argv)

    if argc != 3:
        print '============================================================'
        print 'Usage:'
        print '    python gen_source.py header_file output_source_file'
        print '============================================================'

        sys.exit(0)

    content = read_content(sys.argv[1])
    def_list = parse_content(content)
    write_source(def_list, sys.argv[2])
