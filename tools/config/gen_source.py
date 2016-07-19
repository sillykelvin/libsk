#!/usr/bin/env python

import re
import sys
import string


FIELD_DEFS = {
    's32'    : {'type': 's32', 'conv_func': 'sk::string2s32'},
    'u32'    : {'type': 'u32', 'conv_func': 'sk::string2u32'},
    's64'    : {'type': 's64', 'conv_func': 'sk::string2s64'},
    'u64'    : {'type': 'u64', 'conv_func': 'sk::string2u64'},
    'string' : {'type': 'string', 'conv_func': None},
    'vector' : {'type': 'vector', 'conv_func': None},
    'custom' : {'type': 'custom', 'conv_func': None},
}

class ConfigDef:
    def __init__(self):
        self.name    = None
        self.prefix  = None  # prefix namespace
        self.fields  = []
        self.sub_def = []    # children definitions
        self.sup_def = None  # parent definition

    def __str__(self):
        return self.info(0)

    def full_name(self):
        return self.prefix + self.name

    def info(self, indent):
        ret = '%sDEFINITION: %s\n%sfields:\n' % (' ' * indent, self.full_name(), ' ' * indent)
        for f in self.fields:
            ret += f.info(indent + 4)

        for d in self.sub_def:
            ret += d.info(indent + 4)

        return ret

class FieldInfo:
    def __init__(self):
        self._def  = None        # which def this field belongs
        self._type = None
        self.name  = None
        # will be set only if _type is vector of customized type
        # if the field is std::vector, real_type is the type in vector, if it is scalar
        # type, real_type is a string, otherwise, real_type referenced to ConfigDef instance
        # if the field is customized type, real_type is that type, referenced to ConfigDef instance
        self.real_type = None

    def __str__(self):
        return info(0)

    def info(self, indent):
        ret = '%stype: %s, name: %s' % (' ' * indent, self._type, self.name)
        if self._type['type'] in ('vector', 'custom'):
            assert self.real_type
            if isinstance(self.real_type, str):
                ret += ', real_type: %s' % (self.real_type)
            else:
                assert isinstance(self.real_type, ConfigDef)
                ret += ', real_type: %s' % (self.real_type.full_name())
        ret += '\n'

        return ret

def find_config_def(root_defs, sup_def, type_str):
    for d in sup_def.sub_def:
        if d.name == type_str:
            return d

    if sup_def.sup_def:
        return find_config_def(root_defs, sup_def.sup_def, type_str)

    for d in root_defs:
        if d.name == type_str:
            return d

    return None

def build_config_def(root_defs, parent, name):
    _def = ConfigDef()
    _def.name = name
    if parent:
        _def.sup_def = parent
        _def.prefix = parent.full_name() + '::'
        parent.sub_def.append(_def)
    else:
        _def.prefix = ''
        root_defs.append(_def)

    return _def

def build_field_info(root_defs, sup_def, type_str, name):
    assert sup_def
    field = FieldInfo()
    field._def = sup_def
    field.name = name

    if type_str == 'int':
        type_str = 's32'

    if len(type_str) == 3 and type_str in FIELD_DEFS.keys():
        field._type = FIELD_DEFS[type_str]
    elif type_str == 'std::string':
        field._type = FIELD_DEFS['string']
    elif type_str.startswith('std::vector<'):
        field._type = FIELD_DEFS['vector']
        ret = re.search('^std::vector< *([_a-zA-Z0-9]+) *>$', type_str)
        assert ret
        real_type = ret.group(1)
        if real_type in ('int', 's32', 'u32', 's64', 'u64', 'std::string'):
            if real_type == 'int':
                real_type = 's32'
            field.real_type = real_type
        else:
            field.real_type = find_config_def(root_defs, sup_def, ret.group(1))
            if not field.real_type:
                print 'cannot find type: %s' % (ret.group(1))
                sys.exit(-1)
    else:
        # customize types
        field._type = FIELD_DEFS['custom']
        field.real_type = find_config_def(root_defs, sup_def, type_str)
        if not field.real_type:
            print 'cannot find type: %s' % (type_str)
            sys.exit(-1)

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
            field = build_field_info(def_list, curr_def, type_str, name)
            curr_def.fields.append(field)
            continue

        print "invalid line: %s" % (line)
        sys.exit(-1)

    return def_list

def write_string_load_func():
    return '''
static int load_from_xml_node(std::string& value, const pugi::xml_node& node, const char *node_name) {
    value.clear();
    if (!node) {
        fprintf(stderr, "node %s not found.", node_name);
        return -EINVAL;
    }
    value = node.text().get();

    return 0;
}
'''

def write_integral_load_func(type_str):
    assert len(type_str) == 3
    assert type_str in FIELD_DEFS.keys()
    template = '''
static int load_from_xml_node(${type_name}& value, const pugi::xml_node& node, const char *node_name) {
    value = 0;
    if (!node) {
        fprintf(stderr, "node %s not found.", node_name);
        return -EINVAL;
    }

    int ret = $func_name(node.text().get(), value);
    if (ret != 0) {
        fprintf(stderr, "cannot convert node %s to integer.", node_name);
        return ret;
    }

    return 0;
}
'''
    return string.Template(template).substitute(type_name = FIELD_DEFS[type_str]['type'],
                                                func_name = FIELD_DEFS[type_str]['conv_func'])

def write_vector_load_func(real_type):
    template = '''
static int load_from_xml_node(std::vector<$type_name>& value, const pugi::xml_object_range<pugi::xml_named_node_iterator>& nodes, const char *node_name) {
    value.clear();
    for (auto it = nodes.begin(), end = nodes.end(); it != end; ++it) {
        const pugi::xml_node n = *it;
        $type_name obj;
        int ret = load_from_xml_node(obj, n, node_name);
        if (ret != 0)
            return ret;

        value.push_back(obj);
    }
    if (value.size() <= 0) {
        fprintf(stderr, "node %s not found.", node_name);
        return -EINVAL;
    }

    return 0;
}
'''

    type_name = None
    if isinstance(real_type, str):
        type_name = real_type
    else:
        assert isinstance(real_type, ConfigDef)
        type_name = real_type.full_name()

    return string.Template(template).substitute(type_name = type_name);

def write_custom_load_func(_def):
    func_template = '''
static int load_from_xml_node($type_name& value, const pugi::xml_node& node, const char *node_name) {
    int ret = 0;
    if (!node) {
        fprintf(stderr, "node %s not found.", node_name);
        return -EINVAL;
    }

    // read fields
$fields_src
    return 0;
}
'''

    field_template = '''
    ret = load_from_xml_node(value.$field_name, $node_arg, "$field_name");
    if (ret != 0)
        return ret;
'''

    fields_src = ''
    for field in _def.fields:
        node_arg = 'node.child("%s")' % (field.name)
        if field._type['type'] == 'vector':
            node_arg = 'node.children("%s")' % (field.name)

        fields_src += string.Template(field_template).substitute(field_name = field.name, node_arg = node_arg)

    return string.Template(func_template).substitute(type_name = _def.full_name(), fields_src = fields_src)

def extract_all_types(def_list):
    def recursive(_def, type_dict):
        type_dict[_def.full_name()] = {'type': 'custom', 'real_type': _def}

        for f in _def.fields:
            if f._type['type'] == 'string':
                type_dict['std::string'] = {'type': 'string', 'real_type': None}
            elif f._type['type'] == 'vector':
                assert f.real_type
                if isinstance(f.real_type, str):
                    assert f.real_type in ('s32', 'u32', 's64', 'u64', 'std::string')
                    inner_type = f.real_type
                    if f.real_type == 'std::string':
                        inner_type = 'string'
                    type_dict[f.real_type] = {'type': inner_type, 'real_type': None}

                    type_dict['std::vector<%s>' % (f.real_type)] = {'type': 'vector', 'real_type': f.real_type}
                else:
                    assert isinstance(f.real_type, ConfigDef)
                    type_dict[f.real_type.full_name()] = {'type': 'custom', 'real_type': f.real_type}
                    type_dict['std::vector<%s>' % (f.real_type.full_name())] = {'type': 'vector', 'real_type': f.real_type}
            elif f._type['type'] == 'custom':
                assert f.real_type
                assert isinstance(f.real_type, ConfigDef)
                type_dict[f.real_type.full_name()] = {'type': 'custom', 'real_type': f.real_type}
            else: # integral types
                type_dict[f._type['type']] = {'type': f._type['type'], 'real_type': None}

        for d in _def.sub_def:
            recursive(d, type_dict)

    type_dict = {}
    for d in def_list:
        recursive(d, type_dict)

    return type_dict

def write_declarations(type_dict):
    ret = ''
    for t in type_dict.keys():
        if type_dict[t]['type'] == 'vector':
            ret += 'static int load_from_xml_node(%s& value, const pugi::xml_object_range<pugi::xml_named_node_iterator>& nodes, const char *node_name);\n' % (t)
        else:
            ret += 'static int load_from_xml_node(%s& value, const pugi::xml_node& node, const char *node_name);\n' % (t)

    return ret

def write_source(def_list, filename):
    template = '''
int load_from_xml_file($conf_name& conf, const char *file) {
    assert_retval(file, -1);

    pugi::xml_document doc;
    auto ok = doc.load_file(file);
    if (!ok) {
        fprintf(stderr, "load file %s error: %s.", file, ok.description());
        return -EINVAL;
    }

    pugi::xml_node root = doc.child("$conf_name");
    if (!root) {
        fprintf(stderr, "root node <$conf_name> not found.");
        return -EINVAL;
    }

    return load_from_xml_node(conf, root, "$conf_name");
}
'''

    with open(filename, 'w') as f:
        f.write('// Generated by tool, DO NOT EDIT!!!\n\n')

        type_dict = extract_all_types(def_list)

        # forward declarations
        f.write(write_declarations(type_dict))
        f.write('\n')

        # implementations
        for t in type_dict:
            if type_dict[t]['type'] == 'string':
                f.write(write_string_load_func())
            elif type_dict[t]['type'] == 'vector':
                f.write(write_vector_load_func(type_dict[t]['real_type']))
            elif type_dict[t]['type'] == 'custom':
                assert isinstance(type_dict[t]['real_type'], ConfigDef)
                f.write(write_custom_load_func(type_dict[t]['real_type']))
            else: # integral types
                f.write(write_integral_load_func(t))

        # only first level definitions can be loaded from file
        for _def in def_list:
            f.write(string.Template(template).substitute(conf_name = _def.name))

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
