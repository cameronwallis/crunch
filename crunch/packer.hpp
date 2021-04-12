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
 
 */

#ifndef packer_hpp
#define packer_hpp

#include <vector>
#include <fstream>
#include <unordered_map>
#include "bitmap.hpp"

//using namespace std;

struct Point
{
    int x;
    int y;
    int dupID;
    bool rot;
};

struct Packer
{
    int m_width;
    int m_height;
    int m_pad;
    
    std::vector<Bitmap*> m_bitmaps;
    std::vector<Point> m_points;
    std::unordered_map<size_t, int> m_dupLookup;
    
    Packer(int width, int height, int pad);
    void Pack(std::vector<Bitmap*>& bitmaps, bool verbose, bool unique, bool rotate);
    void SavePng(const std::string& file);
    void SaveXml(const std::string& name, std::ofstream& xml, bool trim, bool rotate);
    void SaveBin(const std::string& name, std::ofstream& bin, bool trim, bool rotate);
    void SaveJson(const std::string& name,std::ofstream& json, bool trim, bool rotate);
};

#endif
