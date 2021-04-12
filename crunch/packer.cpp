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

#include "packer.hpp"
#include "MaxRectsBinPack.h"
#include "GuillotineBinPack.h"
#include "binary.hpp"
#include <iostream>
#include <algorithm>

//using namespace std;
using namespace rbp;

Packer::Packer(int width, int height, int pad)
: m_width(width), m_height(height), m_pad(pad)
{
    
}

void Packer::Pack(std::vector<Bitmap*>& bitmaps, bool verbose, bool unique, bool rotate)
{
    MaxRectsBinPack packer(m_width, m_height);
    
    int ww = 0;
    int hh = 0;
    while (!bitmaps.empty())
    {
        auto bitmap = bitmaps.back();
        
        if (verbose)
            std::cout << '\t' << bitmaps.size() << ": " << bitmap->m_name << std::endl;
        
        //Check to see if this is a duplicate of an already packed bitmap
        if (unique)
        {
            auto di = m_dupLookup.find(bitmap->m_hashValue);
            if (di != m_dupLookup.end() && bitmap->Equals(this->m_bitmaps[di->second]))
            {
                Point p = m_points[di->second];
                p.dupID = di->second;
                m_points.push_back(p);
                this->m_bitmaps.push_back(bitmap);
                bitmaps.pop_back();
                continue;
            }
        }
        
        //If it's not a duplicate, pack it into the atlas
        {
            Rect rect = packer.Insert(bitmap->m_width + m_pad, bitmap->m_height + m_pad, rotate, MaxRectsBinPack::RectBestShortSideFit);
            
            if (rect.width == 0 || rect.height == 0)
                break;
            
            if (unique)
                m_dupLookup[bitmap->m_hashValue] = static_cast<int>(m_points.size());
            
            //Check if we rotated it
            Point p;
            p.x = rect.x;
            p.y = rect.y;
            p.dupID = -1;
            p.rot = rotate && bitmap->m_width != (rect.width - m_pad);
            
            m_points.push_back(p);
            this->m_bitmaps.push_back(bitmap);
            bitmaps.pop_back();
            
            ww = std::max(rect.x + rect.width, ww);
            hh = std::max(rect.y + rect.height, hh);
        }
    }
    
    while (m_width / 2 >= ww)
        m_width /= 2;
    while( m_height / 2 >= hh)
        m_height /= 2;
}

void Packer::SavePng(const std::string& file)
{
    Bitmap bitmap(m_width, m_height);
    for (size_t i = 0, j = m_bitmaps.size(); i < j; ++i)
    {
        if (m_points[i].dupID < 0)
        {
            if (m_points[i].rot)
                bitmap.CopyPixelsRot(m_bitmaps[i], m_points[i].x, m_points[i].y);
            else
                bitmap.CopyPixels(m_bitmaps[i], m_points[i].x, m_points[i].y);
        }
    }
    bitmap.SaveAs(file);
}

void Packer::SaveXml(const std::string& name, std::ofstream& xml, bool trim, bool rotate)
{
    xml << "\t<tex n=\"" << name << "\">" << std::endl;
    for (size_t i = 0, j = m_bitmaps.size(); i < j; ++i)
    {
        xml << "\t\t<img n=\"" << m_bitmaps[i]->m_name << "\" ";
        xml << "x=\"" << m_points[i].x << "\" ";
        xml << "y=\"" << m_points[i].y << "\" ";
        xml << "w=\"" << m_bitmaps[i]->m_width << "\" ";
        xml << "h=\"" << m_bitmaps[i]->m_height << "\" ";
        if (trim)
        {
            xml << "fx=\"" << m_bitmaps[i]->m_frameX << "\" ";
            xml << "fy=\"" << m_bitmaps[i]->m_frameY << "\" ";
            xml << "fw=\"" << m_bitmaps[i]->m_frameW << "\" ";
            xml << "fh=\"" << m_bitmaps[i]->m_frameH << "\" ";
        }
        if (rotate)
            xml << "r=\"" << (m_points[i].rot ? 1 : 0) << "\" ";
        xml << "/>" << std::endl;
    }
    xml << "\t</tex>" << std::endl;
}

void Packer::SaveBin(const std::string& name, std::ofstream& bin, bool trim, bool rotate)
{
    WriteString(bin, name);
    WriteShort(bin, static_cast<int16_t>(m_bitmaps.size()));
    for (size_t i = 0, j = m_bitmaps.size(); i < j; ++i)
    {
        WriteString(bin, m_bitmaps[i]->m_name);
        WriteShort(bin, static_cast<int16_t>(m_points[i].x));
        WriteShort(bin, static_cast<int16_t>(m_points[i].y));
        WriteShort(bin, static_cast<int16_t>(m_bitmaps[i]->m_width));
        WriteShort(bin, static_cast<int16_t>(m_bitmaps[i]->m_height));
        if (trim)
        {
            WriteShort(bin, static_cast<int16_t>(m_bitmaps[i]->m_frameX));
            WriteShort(bin, static_cast<int16_t>(m_bitmaps[i]->m_frameY));
            WriteShort(bin, static_cast<int16_t>(m_bitmaps[i]->m_frameW));
            WriteShort(bin, static_cast<int16_t>(m_bitmaps[i]->m_frameH));
        }
        if (rotate)
            WriteByte(bin, m_points[i].rot ? 1 : 0);
    }
}

void Packer::SaveJson(const std::string& name, std::ofstream& json, bool trim, bool rotate)
{
    json << "\t\t\t\"name\":\"" << name << "\"," << std::endl;
    json << "\t\t\t\"images\":[" << std::endl;
    for (size_t i = 0, j = m_bitmaps.size(); i < j; ++i)
    {
        json << "\t\t\t\t{ ";
        json << "\"n\":\"" << m_bitmaps[i]->m_name << "\", ";
        json << "\"x\":" << m_points[i].x << ", ";
        json << "\"y\":" << m_points[i].y << ", ";
        json << "\"w\":" << m_bitmaps[i]->m_width << ", ";
        json << "\"h\":" << m_bitmaps[i]->m_height;
        if (trim)
        {
            json << ", \"fx\":" << m_bitmaps[i]->m_frameX << ", ";
            json << "\"fy\":" << m_bitmaps[i]->m_frameY << ", ";
            json << "\"fw\":" << m_bitmaps[i]->m_frameW << ", ";
            json << "\"fh\":" << m_bitmaps[i]->m_frameH;
        }
        if (rotate)
            json << ", \"r\":" << (m_points[i].rot ? "true" : "false");
        json << " }";
        if(i != m_bitmaps.size() -1)
            json << ",";
        json << std::endl;
    }
    json << "\t\t\t]" << std::endl;
}
