#include <iostream>
#include <regex>
#include <string>
#include <vector>

int main()
{
    std::vector<std::string> vga_audio_pairs;
    std::vector<std::string> vga_device_names;
    std::vector<std::string> vfio_groups;
    bool amd_found = false;
    std::string last_vga_id;
    std::string last_vga_name;
    int i = 0;
    int choice;

    FILE *pipe = popen("lspci -nn | grep -E 'VGA|Audio|Display'", "r");
    if (!pipe)
    {
        std::cerr << "failed to run lspci command!" << std::endl;
        return 1;
    }

    char buffer[256];
    std::regex vga_regex(R"(VGA compatible controller \[0300\]: (.*)\s+\[(\w{4}:\w{4})\])");
    std::regex audio_regex("Audio device.*\\[(\\w{4}:\\w{4})\\]");
    std::smatch match;
    bool expecting_audio = false;

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
        std::string line(buffer);

        if (line.find("AMD") != std::string::npos)
        {
            amd_found = true;
        }

        if (std::regex_search(line, match, vga_regex))
        {
            last_vga_name = match.str(1);
            last_vga_id = match.str(2);
            vga_device_names.push_back(last_vga_name);
            expecting_audio = true;
        }
        else if (expecting_audio && std::regex_search(line, match, audio_regex))
        {
            std::string audio_id = match.str(1);
            vga_audio_pairs.push_back(last_vga_id + "," + audio_id);
            expecting_audio = false;
        }
    }
    pclose(pipe);

    for (const auto &pair : vga_audio_pairs)
    {
        std::string vfio_entry;
        if (pair.find("1002:") != std::string::npos || pair.find("1022:") != std::string::npos)
        {
            vfio_entry = "options vfio-pci ids=" + pair + "\nsoftdep amdgpu pre: vfio-pci";
        }
        else
        {
            vfio_entry = "options vfio-pci ids=" + pair + "\nsoftdep nvidia pre: vfio-pci";
        }
        vfio_groups.push_back(vfio_entry);
    }

    for (const auto &name : vga_device_names)
    {
        std::cout << (i + 1) << ". " << name << std::endl;
        i++;
    }

    std::cout << (i + 1) << ". unhook your GPU from vfio-pci" << std::endl;

    std::cout << "Select a device you wish to passthrough: ";

    while (true)
    {
        std::cin >> choice;
        if (choice >= 1 && choice <= vga_device_names.size())
        {
            system("sudo rm /etc/modprobe.d/vfio.conf");
            std::string command = "echo \"" + vfio_groups[choice - 1] + "\" | sudo tee -a /etc/modprobe.d/vfio.conf";
            system(command.c_str());
            system("sudo update-initramfs -c -k $(uname -r)");
            std::cout << vga_device_names[choice - 1] + " configured for vfio-pci" << std::endl;
            break;
        }
        system("sudo rm /etc/modprobe.d/vfio.conf");
        system("sudo update-initramfs -c -k $(uname -r)");
        std::cout << "GPU unhooked from vfio-pci" << std::endl;
        break;
    }

    return 0;
}
