/*
 
 MIT License
 
 Copyright (c) 2017 Chevy Ray Johnston
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
 
 crunch - command line texture packer
 ====================================
 
 usage:
    crunch [OUTPUT] [INPUT1,INPUT2,INPUT3...] [OPTIONS...]
 
 example:
    crunch bin/atlases/atlas assets/characters,assets/tiles -p -t -v -u -r
 
 options:
    -d  --default           use default settings (-x -p -t -u)
    -x  --xml               saves the atlas data as a .xml file
    -b  --binary            saves the atlas data as a .bin file
    -j  --json              saves the atlas data as a .json file
    -p  --premultiply       premultiplies the pixels of the bitmaps by their alpha channel
    -t  --trim              trims excess transparency off the bitmaps
    -v  --verbose           print to the debug console as the packer works
    -f  --force             ignore the hash, forcing the packer to repack
    -u  --unique            remove duplicate bitmaps from the atlas
    -r  --rotate            enabled rotating bitmaps 90 degrees clockwise when packing
    -s# --size#             max atlas size (# can be 4096, 2048, 1024, 512, 256, 128, or 64)
    -p# --pad#              padding between images (# can be from 0 to 16)
 
 binary format:
    [int16] num_textures (below block is repeated this many times)
        [string] name
        [int16] num_images (below block is repeated this many times)
            [string] img_name
            [int16] img_x
            [int16] img_y
            [int16] img_width
            [int16] img_height
            [int16] img_frame_x         (if --trim enabled)
            [int16] img_frame_y         (if --trim enabled)
            [int16] img_frame_width     (if --trim enabled)
            [int16] img_frame_height    (if --trim enabled)
            [byte] img_rotated          (if --rotate enabled)
 */

#include <iostream>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include "tinydir.h"
#include "bitmap.hpp"
#include "packer.hpp"
#include "binary.hpp"
#include "hash.hpp"
#include "str.hpp"

//using namespace std;

static int optSize;
static int optPadding;
static bool optXml;
static bool optBinary;
static bool optJson;
static bool optPremultiply;
static bool optTrim;
static bool optVerbose;
static bool optForce;
static bool optUnique;
static bool optRotate;
static std::vector<Bitmap*> bitmaps;
static std::vector<Packer*> packers;

static void SplitFileName(const std::string& path, std::string* dir, std::string* name, std::string* ext)
{
    size_t si = path.rfind('/') + 1;
    if (si == std::string::npos)
        si = 0;
    size_t di = path.rfind('.');
    if (dir != nullptr)
    {
        if (si > 0)
            *dir = path.substr(0, si);
        else
            *dir = "";
    }
    if (name != nullptr)
    {
        if (di != std::string::npos)
            *name = path.substr(si, di - si);
        else
            *name = path.substr(si);
    }
    if (ext != nullptr)
    {
        if (di != std::string::npos)
            *ext = path.substr(di);
        else
            *ext = "";
    }
}

static std::string GetFileName(const std::string& path)
{
    std::string name;
    SplitFileName(path, nullptr, &name, nullptr);
    return name;
}

static void LoadBitmap(const std::string& prefix, const std::string& path)
{
    if (optVerbose)
        std::cout << '\t' << PathToStr(path) << std::endl;
    
    bitmaps.push_back(new Bitmap(PathToStr(path), prefix + GetFileName(PathToStr(path)), optPremultiply, optTrim));
}

static void LoadBitmaps(const std::string& root, const std::string& prefix)
{
    static std::string dot1 = ".";
    static std::string dot2 = "..";
    
    tinydir_dir dir;
    tinydir_open(&dir, StrToPath(root).data());
    
    while (dir.has_next)
    {
        tinydir_file file;
        tinydir_readfile(&dir, &file);
        
        if (file.is_dir)
        {
            if (dot1 != PathToStr(file.name) && dot2 != PathToStr(file.name))
                LoadBitmaps(PathToStr(file.path), prefix + PathToStr(file.name) + "/");
        }
        else if (PathToStr(file.extension) == "png")
            LoadBitmap(prefix, file.path);
        
        tinydir_next(&dir);
    }
    
    tinydir_close(&dir);
}

static void RemoveFile(std::string file)
{
    remove(file.data());
}

static int GetPackSize(const std::string& str)
{
    if (str == "4096")
        return 4096;
    if (str == "2048")
        return 2048;
    if (str == "1024")
        return 1024;
    if (str == "512")
        return 512;
    if (str == "256")
        return 256;
    if (str == "128")
        return 128;
    if (str == "64")
        return 64;
    std::cerr << "invalid size: " << str << std::endl;
    exit(EXIT_FAILURE);
    return 0;
}

static int GetPadding(const std::string& str)
{
    for (int i = 0; i <= 16; ++i)
        if (str == std::to_string(i))
            return i;
    std::cerr << "invalid padding value: " << str << std::endl;
    exit(EXIT_FAILURE);
    return 1;
}

// TODO: Review main function
int main(int argc, const char* argv[])
{
    //Print out passed arguments
    for (int i = 0; i < argc; ++i)
        std::cout << argv[i] << ' ';
    std::cout << std::endl;
    
    if (argc < 3)
    {
        std::cerr << "invalid input, expected: \"crunch [INPUT DIRECTORY] [OUTPUT PREFIX] [OPTIONS...]\"" << std::endl;
        return EXIT_FAILURE;
    }
    
    //Get the output directory and name
    std::string outputDir, name;
    SplitFileName(argv[1], &outputDir, &name, nullptr);
    
    //Get all the input files and directories
    std:: vector<std::string> inputs;
    std::stringstream ss(argv[2]);
    while (ss.good())
    {
        std::string inputStr;
        getline(ss, inputStr, ',');
        inputs.push_back(inputStr);
    }
    
    //Get the options
    optSize = 4096;
    optPadding = 1;
    optXml = false;
    optBinary = false;
    optJson = false;
    optPremultiply = false;
    optTrim = false;
    optVerbose = false;
    optForce = false;
    optUnique = false;
    for (int i = 3; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "-d" || arg == "--default")
            optXml = optPremultiply = optTrim = optUnique = true;
        else if (arg == "-x" || arg == "--xml")
            optXml = true;
        else if (arg == "-b" || arg == "--binary")
            optBinary = true;
        else if (arg == "-j" || arg == "--json")
            optJson = true;
        else if (arg == "-p" || arg == "--premultiply")
            optPremultiply = true;
        else if (arg == "-t" || arg == "--trim")
            optTrim = true;
        else if (arg == "-v" || arg == "--verbose")
            optVerbose = true;
        else if (arg == "-f" || arg == "--force")
            optForce = true;
        else if (arg == "-u" || arg == "--unique")
            optUnique = true;
        else if (arg == "-r" || arg == "--rotate")
            optRotate = true;
        else if (arg.find("--size") == 0)
            optSize = GetPackSize(arg.substr(6));
        else if (arg.find("-s") == 0)
            optSize = GetPackSize(arg.substr(2));
        else if (arg.find("--pad") == 0)
            optPadding = GetPadding(arg.substr(5));
        else if (arg.find("-p") == 0)
            optPadding = GetPadding(arg.substr(2));
        else
        {
            std::cerr << "unexpected argument: " << arg << std::endl;
            return EXIT_FAILURE;
        }
    }
    
    //Hash the arguments and input directories
    size_t newHash = 0;
    for (int i = 1; i < argc; ++i)
        HashString(newHash, argv[i]);
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (inputs[i].rfind('.') == std::string::npos)
            HashFiles(newHash, inputs[i]);
        else
            HashFile(newHash, inputs[i]);
    }
    
    //Load the old hash
    size_t oldHash;
    if (LoadHash(oldHash, outputDir + name + ".hash"))
    {
        if (!optForce && newHash == oldHash)
        {
            std::cout << "atlas is unchanged: " << name << std::endl;
            return EXIT_SUCCESS;
        }
    }
    
    /*-d  --default           use default settings (-x -p -t -u)
    -x  --xml               saves the atlas data as a .xml file
    -b  --binary            saves the atlas data as a .bin file
    -j  --json              saves the atlas data as a .json file
    -p  --premultiply       premultiplies the pixels of the bitmaps by their alpha channel
    -t  --trim              trims excess transparency off the bitmaps
    -v  --verbose           print to the debug console as the packer works
    -f  --force             ignore the hash, forcing the packer to repack
    -u  --unique            remove duplicate bitmaps from the atlas
    -r  --rotate            enabled rotating bitmaps 90 degrees clockwise when packing
    -s# --size#             max atlas size (# can be 4096, 2048, 1024, 512, or 256)
    -p# --pad#              padding between images (# can be from 0 to 16)*/
    
    if (optVerbose)
    {
        std::cout << "options..." << std::endl;
        std::cout << "\t--xml: " << (optXml ? "true" : "false") << std::endl;
        std::cout << "\t--binary: " << (optBinary ? "true" : "false") << std::endl;
        std::cout << "\t--json: " << (optJson ? "true" : "false") << std::endl;
        std::cout << "\t--premultiply: " << (optPremultiply ? "true" : "false") << std::endl;
        std::cout << "\t--trim: " << (optTrim ? "true" : "false") << std::endl;
        std::cout << "\t--verbose: " << (optVerbose ? "true" : "false") << std::endl;
        std::cout << "\t--force: " << (optForce ? "true" : "false") << std::endl;
        std::cout << "\t--unique: " << (optUnique ? "true" : "false") << std::endl;
        std::cout << "\t--rotate: " << (optRotate ? "true" : "false") << std::endl;
        std::cout << "\t--size: " << optSize << std::endl;
        std::cout << "\t--pad: " << optPadding << std::endl;
    }
    
    //Remove old files
    RemoveFile(outputDir + name + ".hash");
    RemoveFile(outputDir + name + ".bin");
    RemoveFile(outputDir + name + ".xml");
    RemoveFile(outputDir + name + ".json");
    for (size_t i = 0; i < 16; ++i)
        RemoveFile(outputDir + name + std::to_string(i) + ".png");
    
    //Load the bitmaps from all the input files and directories
    if (optVerbose)
        std::cout << "loading images..." << std::endl;
    for (size_t i = 0; i < inputs.size(); ++i)
    {
        if (inputs[i].rfind('.') != std::string::npos)
            LoadBitmap("", inputs[i]);
        else
            LoadBitmaps(inputs[i], "");
    }
    
    //Sort the bitmaps by area
    sort(bitmaps.begin(), bitmaps.end(), [](const Bitmap* a, const Bitmap* b) {
        return (a->width * a->height) < (b->width * b->height);
    });
    
    //Pack the bitmaps
    while (!bitmaps.empty())
    {
        if (optVerbose)
            std::cout << "packing " << bitmaps.size() << " images..." << std::endl;
        auto packer = new Packer(optSize, optSize, optPadding);
        packer->Pack(bitmaps, optVerbose, optUnique, optRotate);
        packers.push_back(packer);
        if (optVerbose)
            std::cout << "finished packing: " << name << std::to_string(packers.size() - 1) << " (" << packer->width << " x " << packer->height << ')' << std::endl;
    
        if (packer->bitmaps.empty())
        {
            std::cerr << "packing failed, could not fit bitmap: " << (bitmaps.back())->name << std::endl;
            return EXIT_FAILURE;
        }
    }
    
    //Save the atlas image
    for (size_t i = 0; i < packers.size(); ++i)
    {
        if (optVerbose)
            std::cout << "writing png: " << outputDir << name << std::to_string(i) << ".png" << std::endl;
        packers[i]->SavePng(outputDir + name + std::to_string(i) + ".png");
    }
    
    //Save the atlas binary
    if (optBinary)
    {
        if (optVerbose)
            std::cout << "writing bin: " << outputDir << name << ".bin" << std::endl;
        
        std::ofstream bin(outputDir + name + ".bin", std::ios::binary);
        WriteShort(bin, (int16_t)packers.size());
        for (size_t i = 0; i < packers.size(); ++i)
            packers[i]->SaveBin(name + std::to_string(i), bin, optTrim, optRotate);
        bin.close();
    }
    
    //Save the atlas xml
    if (optXml)
    {
        if (optVerbose)
            std::cout << "writing xml: " << outputDir << name << ".xml" << std::endl;
        
        std::ofstream xml(outputDir + name + ".xml");
        xml << "<atlas>" << std::endl;
        for (size_t i = 0; i < packers.size(); ++i)
            packers[i]->SaveXml(name + std::to_string(i), xml, optTrim, optRotate);
        xml << "</atlas>";
    }
    
    //Save the atlas json
    if (optJson)
    {
        if (optVerbose)
            std::cout << "writing json: " << outputDir << name << ".json" << std::endl;
        
        std::ofstream json(outputDir + name + ".json");
        json << '{' << std::endl;
        json << "\t\"textures\":[" << std::endl;
        for (size_t i = 0; i < packers.size(); ++i)
        {
            json << "\t\t{" << std::endl;
            packers[i]->SaveJson(name + std::to_string(i), json, optTrim, optRotate);
            json << "\t\t}";
            if (i + 1 < packers.size())
                json << ',';
            json << std::endl;
        }
        json << "\t]" << std::endl;
        json << '}';
    }
    
    //Save the new hash
    SaveHash(newHash, outputDir + name + ".hash");
    
    return EXIT_SUCCESS;
}
