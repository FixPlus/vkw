import xml.etree.ElementTree as xmlReader
import argparse


class TypeDesc:
    def __init__(self):
        self.type = ''
        self.creator = ''
        self.constructor = ''
        self.destructor = ''
        self.createInfoType = ''

    def generate(self):
        print('#ifdef VKW_GENERATE_TYPE_DEFINITIONS')
        print('template<>')
        print('struct VulkanTypeTraits<' + self.type + '> {')
        print('   using CreatorType = ' + self.creator + ';')
        print('   using CreateInfoType = ' + self.createInfoType + ';')
        print('   static PFN_' + self.constructor + ' getConstructor(' + self.creator + ' const& creator);')
        print('   static PFN_' + self.destructor + ' getDestructor(' + self.creator + ' const& creator);')
        print('};')
        print('#endif')
        print('#ifdef VKW_GENERATE_TYPE_FUNC_IMPL')
        print(
            'inline PFN_' + self.constructor + ' VulkanTypeTraits<' + self.type + '>::getConstructor(' + self.creator + ' const& creator) {')
        print('   return creator.core<1, 0>().' + self.constructor + ';')
        print('}')
        print(
            'inline PFN_' + self.destructor + ' VulkanTypeTraits<' + self.type + '>::getDestructor(' + self.creator + ' const& creator) {')
        print('   return creator.core<1, 0>().' + self.destructor + ';')
        print('}')
        print('#endif')
        print('#ifdef VKW_GENERATE_ALIAS_TYPES')
        print('namespace vk{')
        print('using ' + self.type[2:] + ' = Unique<' + self.type + '>;')
        print('} // namespace vk')
        print('#endif')
        print('')

def fill_Type_desc(command_desc):
    ret = TypeDesc()
    ret.constructor = command.find('proto').find('name').text
    ret.destructor = ret.constructor.replace('vkCreate', 'vkDestroy')
    ret.type = ret.constructor.replace('vkCreate', 'Vk')
    param_creator = command.find('param').find('type').text
    if param_creator == 'VkDevice':
        ret.creator = 'vkw::Device'
    else:
        if param_creator == 'VkInstance':
            ret.creator = 'vkw::Instance'
        else:
            ret.creator = 'void'

    for param in command.findall('param'):
        param_type = param.find('type').text
        if 'CreateInfo' in param_type:
            ret.createInfoType = param_type
    return ret

def command_filter(command):
    if not command.attrib.get('export'):
        return False
    return 'vulkan' in command.attrib.get('export').split(',')

if __name__ == '__main__':
    args = argparse.ArgumentParser()

    args.add_argument('-path', action='store', default='.',
                      help='path to vk.xml')

    parsed_args = args.parse_args()

    doc = xmlReader.parse(parsed_args.path + '/vk.xml')

    types = []
    root = doc.getroot()
    for command in filter(command_filter, root.find('commands')):
        if command.find('proto') is None:
            continue
        name = command.find('proto').find('name').text
        if name.startswith('vkCreate'):
            types.append(fill_Type_desc(command))

    for t in types:
        if t.creator == 'void':
            continue
        for core_command in root.find('feature').iter('command'):
            if core_command.attrib.get('name') == t.constructor:
                for core_type in root.find('feature').iter('type'):
                    if core_type.attrib.get('name') == t.type:
                        t.generate()
