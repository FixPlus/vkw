import xml.etree.ElementTree as xmlReader
import argparse


def generate_feature_entry(feature):
    feature_name = feature.find('name').text
    return "VKW_FEATURE_ENTRY(" + feature_name + ")\n"


if __name__ == '__main__':
    args = argparse.ArgumentParser()

    args.add_argument('-path', action='store', default='.',
                      help='path to vk.xml')

    parsed_args = args.parse_args()

    doc = xmlReader.parse(parsed_args.path + '/vk.xml')

    output = ""
    for vk_type in doc.getroot().find('types').findall('type'):
        if vk_type.attrib.get('name') != 'VkPhysicalDeviceFeatures':
            continue
        for feature in vk_type.findall('member'):
            output += generate_feature_entry(feature)
        break
    print("#ifdef VKW_DUMP_FEATURES")
    print(output)
    print("#endif")
    output = ""
    for vk_type in doc.getroot().find('types').findall('type'):
        if vk_type.attrib.get('name') != 'VkPhysicalDeviceVulkan11Features':
            continue
        for feature in filter(lambda n: n.find('name').text != 'sType' and n.find('name').text != 'pNext',
                              vk_type.findall('member')):
            output += generate_feature_entry(feature)
        break

    print("#ifdef VKW_DUMP_VULKAN11_FEATURES")
    print(output)
    print("#endif")
