#include "stdafx.h"

#include "FileSystem.h"
#include "xrCore/xr_token.h"

#include "fs_internal.h"

#include <functional>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>

XRCORE_API CInifile const* pSettings = nullptr;
XRCORE_API CInifile const* pSettingsAuth = nullptr;
XRCORE_API CInifile const* pSettingsOpenXRay = nullptr;

#if defined(XR_PLATFORM_LINUX) || defined(XR_PLATFORM_BSD) || defined(XR_PLATFORM_APPLE)
#include <stdint.h>
#define MSVCRT_EINVAL 22
#define MSVCRT_ERANGE 34

#define MSVCRT_UI64_MAX (((uint64_t)0xffffffff << 32) | 0xffffffff)

/**
 * from wine@dlls/msvcrt/string.c
 *
 * @param fileName
 * @param readOnly
 * @return
 */
int _cdecl _i64toa_s(int64_t value, char* str, size_t size, int radix)
{
    uint64_t val;
    unsigned int digit;
    int is_negative;
    char buffer[65], *pos;
    size_t len;

    if (!(str != NULL))
        return MSVCRT_EINVAL;
    if (!(size > 0))
        return MSVCRT_EINVAL;
    if (!(radix >= 2 && radix <= 36))
    {
        str[0] = '\0';
        return MSVCRT_EINVAL;
    }

    if (value < 0 && radix == 10)
    {
        is_negative = 1;
        val = -value;
    }
    else
    {
        is_negative = 0;
        val = value;
    }

    pos = buffer + 64;
    *pos = '\0';

    do
    {
        digit = val % radix;
        val /= radix;

        if (digit < 10)
            *--pos = '0' + digit;
        else
            *--pos = 'a' + digit - 10;
    } while (val != 0);

    if (is_negative)
        *--pos = '-';

    len = buffer + 65 - pos;
    if (len > size)
    {
        size_t i;
        char* p = str;

        /* Copy the temporary buffer backwards up to the available number of
         * characters. Don't copy the negative sign if present. */

        if (is_negative)
        {
            p++;
            size--;
        }

        for (pos = buffer + 63, i = 0; i < size; i++)
            *p++ = *pos--;

        str[0] = '\0';
        return MSVCRT_ERANGE;
    }

    memcpy(str, pos, len);
    return 0;
}

int _cdecl _ui64toa_s(uint64_t value, char* str, size_t size, int radix)
{
    char buffer[65], *pos;
    int digit;

    if (!(str != NULL))
        return MSVCRT_EINVAL;
    if (!(size > 0))
        return MSVCRT_EINVAL;
    if (!(radix >= 2 && radix <= 36))
    {
        str[0] = '\0';
        return MSVCRT_EINVAL;
    }

    pos = buffer + 64;
    *pos = '\0';

    do
    {
        digit = value % radix;
        value /= radix;

        if (digit < 10)
            *--pos = '0' + digit;
        else
            *--pos = 'a' + digit - 10;
    } while (value != 0);

    if (static_cast<size_t>(buffer - pos + 65) > size)
    {
        return MSVCRT_EINVAL;
    }

    memcpy(str, pos, buffer - pos + 65);
    return 0;
}

LARGE_INTEGER _cdecl _atoi64(const char* str)
{
    ULARGE_INTEGER RunningTotal = 0;
    char bMinus = 0;

    while (*str == ' ' || (*str >= '\011' && *str <= '\015'))
    {
        str++;
    } /* while */

    if (*str == '+')
        str++;
    else if (*str == '-')
    {
        bMinus = 1;
        str++;
    } /* if */

    while (*str >= '0' && *str <= '9')
    {
        RunningTotal = RunningTotal * 10 + *str - '0';
        str++;
    } /* while */

    return bMinus ? -RunningTotal : RunningTotal;
}

uint64_t _cdecl _strtoui64_l(const char* nptr, char** endptr, int base, locale_t locale)
{
    BOOL negative = FALSE;
    uint64_t ret = 0;

    if (!(nptr != NULL))
        return 0;
    if (!(base == 0 || base >= 2))
        return 0;
    if (!(base <= 36))
        return 0;

    while (isspace(*nptr))
        nptr++;

    if (*nptr == '-')
    {
        negative = TRUE;
        nptr++;
    }
    else if (*nptr == '+')
        nptr++;

    if ((base == 0 || base == 16) && *nptr == '0' && tolower(*(nptr + 1)) == 'x')
    {
        base = 16;
        nptr += 2;
    }

    if (base == 0)
    {
        if (*nptr == '0')
            base = 8;
        else
            base = 10;
    }

    while (*nptr)
    {
        char cur = tolower(*nptr);
        int v;

        if (isdigit(cur))
        {
            if (cur >= '0' + base)
                break;
            v = *nptr - '0';
        }
        else
        {
            if (cur < 'a' || cur >= 'a' + base - 10)
                break;
            v = cur - 'a' + 10;
        }

        nptr++;

        if (ret > MSVCRT_UI64_MAX / base || ret * base > MSVCRT_UI64_MAX - v)
            ret = MSVCRT_UI64_MAX;
        else
            ret = ret * base + v;
    }

    if (endptr)
        *endptr = (char*)nptr;

    return negative ? -ret : ret;
}

uint64_t _cdecl _strtoui64(const char* nptr, char** endptr, int base) { return _strtoui64_l(nptr, endptr, base, NULL); }
#endif

CInifile* CInifile::Create(pcstr fileName, bool readOnly) { return xr_new<CInifile>(fileName, readOnly); }

void CInifile::Destroy(CInifile* ini) { xr_delete(ini); }

bool sect_pred(const CInifile::Sect* x, pcstr val) { return xr_strcmp(*x->Name, val) < 0; }

bool item_pred(const CInifile::Item& x, pcstr val)
{
    if (!x.first || !val)
        return x.first < val;
    return xr_strcmp(*x.first, val) < 0;
}

XRCORE_API bool _parse(pstr dest, pcstr src)
{
    bool bInsideSTR = false;
    if (src)
    {
        while (*src)
        {
            if (isspace((u8)*src))
            {
                if (bInsideSTR)
                {
                    *dest++ = *src++;
                    continue;
                }

                while (*src && isspace(*src))
                    ++src;

                continue;
            }

            if (*src == '"')
                bInsideSTR = !bInsideSTR;

            *dest++ = *src++;
        }
    }
    *dest = 0;
    return bInsideSTR;
}

XRCORE_API void _decorate(pstr dest, pcstr src)
{
    if (src)
    {
        bool bInsideSTR = false;
        while (*src)
        {
            if (*src == ',')
            {
                if (bInsideSTR)
                    *dest++ = *src++;
                else
                {
                    *dest++ = *src++;
                    *dest++ = ' ';
                }

                continue;
            }
            if (*src == '"')
                bInsideSTR = !bInsideSTR;

            *dest++ = *src++;
        }
    }
    *dest = 0;
}
//------------------------------------------------------------------------------

bool CInifile::Sect::line_exist(pcstr line, pcstr* value)
{
    auto A = std::lower_bound(Data.begin(), Data.end(), line, item_pred);
    if (A != Data.end() && xr_strcmp(*A->first, line) == 0)
    {
        if (value)
            *value = *A->second;
        return true;
    }
    return false;
}
//------------------------------------------------------------------------------

CInifile::CInifile(IReader* F, pcstr path, allow_include_func_t allow_include_func)
{
    m_file_name[0] = 0;
    m_flags.zero();
    m_flags.set(eSaveAtEnd, false);
    m_flags.set(eReadOnly, true);
    m_flags.set(eOverrideNames, false);
    Load(F, path, allow_include_func);
}

CInifile::CInifile(pcstr fileName, bool readOnly, bool loadAtStart, bool saveAtEnd, u32 sect_count,
    allow_include_func_t allow_include_func)
{
    if (fileName && strstr(fileName, "system"))
        Msg("-----loading %s", fileName);

    m_file_name[0] = 0;
    m_flags.zero();
    if (fileName)
        xr_strcpy(m_file_name, sizeof m_file_name, fileName);

    m_flags.set(eSaveAtEnd, saveAtEnd);
    m_flags.set(eReadOnly, readOnly);

    if (loadAtStart)
    {
        IReader* R = FS.r_open(fileName);
        if (R)
        {
            const xr_string path = EFS_Utils::ExtractFilePath(m_file_name);
            if (sect_count)
                DATA.reserve(sect_count);
            Load(R, path.c_str(), allow_include_func);
            FS.r_close(R);
        }
    }
}

CInifile::~CInifile()
{

    if (!m_flags.test(eReadOnly) && m_flags.test(eSaveAtEnd) && !save_as())
        Log("!Can't save inifile:", m_file_name);

    for (auto* val : DATA)
    {
        xr_delete(val);
    }
}

static void insert_item(CInifile::Sect* tgt, const CInifile::Item& I)
{
    auto sect_it = std::lower_bound(tgt->Data.begin(), tgt->Data.end(), *I.first, item_pred);
    if (sect_it != tgt->Data.end() && sect_it->first.equal(I.first))
    {
        sect_it->second = I.second;
    }
    else
        tgt->Data.insert(sect_it, I);
}

static void insert_item_ignore_duplicates(CInifile::Sect* tgt, const CInifile::Item& I)
{
    auto sect_it = std::lower_bound(tgt->Data.begin(), tgt->Data.end(), *I.first, item_pred);
    if (sect_it != tgt->Data.end() && sect_it->first.equal(I.first))
    {
        return;
    }
    else
    {
        tgt->Data.insert(sect_it, I);
    }
}

IC bool is_empty_line_now(IReader* F)
{
    char* a0 = (char*)F->pointer() - 4;
    char* a1 = (char*)F->pointer() - 3;
    char* a2 = (char*)F->pointer() - 2;
    char* a3 = (char*)F->pointer() - 1;

    return *a0 == 13 && *a1 == 10 && *a2 == 13 && *a3 == 10;
};

std::vector<std::string> Checked_files;

void CInifile::Load(IReader* F, pcstr path, allow_include_func_t allow_include_func)
{
    R_ASSERT(F);

    std::string OLTX_DELETE = "OLTX_DELETE";


    std::unordered_map<shared_str, Sect*> OLTXData;
    std::unordered_map<shared_str, std::vector<shared_str>> IncludeParents;
    std::unordered_map<shared_str, std::vector<shared_str>> Parents;
    std::vector<shared_str> CreateSec;
    std::vector<shared_str> DeleteSec;
    std::unordered_map<shared_str, std::vector<std::pair<std::string, std::string>>> ModData; // section - vector of (key - value)

    std::function<void(Sect*)> StoreSection = [&](Sect * Section)
    { 
            OLTXData[Section->Name] = Section;
    };
    
    std::unordered_map<shared_str, std::vector<shared_str>> FinalParents;
    
    std::function<u32(u32, shared_str, shared_str)> ParentsLoad = [&](u32 total_count, shared_str element, shared_str root_element) 
    {
        if (section_exist(element))
        {
            FinalParents[root_element].push_back(element);

            Sect& inherited_section = r_section(element);
            total_count += inherited_section.Data.size();
        }
        else if (OLTXData.find(element) != OLTXData.end())
        {
            Sect& inherited_section = *OLTXData[element];
            shared_str sec_name = (&inherited_section)->Name;
            if (FinalParents.find(sec_name) == FinalParents.end())
            {
                FinalParents[root_element].push_back(sec_name);
            }
            else
            {
                xrDebug::Fatal(DEBUG_INFO, "Recursive parents for section '%s' was found.", root_element.c_str());
            }

            if (!section_exist(sec_name))
            {
                for (shared_str parent : IncludeParents[sec_name])
                {
                    total_count = ParentsLoad(total_count, parent, root_element);
                }
            }

            total_count += inherited_section.Data.size();
        }
        else
        {
            xrDebug::Fatal(DEBUG_INFO, "Undefined parent '%s' was found.", element.c_str());
        }
        return total_count;
    };
    
    std::function<void()> StarIncluding = [&]() {
        for (auto it = OLTXData.begin(); it != OLTXData.end(); ++it)
        {
            if (&IncludeParents[it->first] != nullptr)
            {
                std::vector<shared_str> element = IncludeParents[it->first];
                u32 j, total_count = 0;
                for (j = 0; j < element.size(); ++j)
                {
                    total_count += ParentsLoad(total_count, element[j], it->first);
                }

                it->second->Data.reserve(it->second->Data.size() + total_count);

                element = FinalParents[it->first];
                for (j = 0; j < element.size(); ++j)
                {
                    if (section_exist(element[j]))
                    {
                        Sect& inherited_section = r_section(element[j]);
                        for (auto it2 = inherited_section.Data.begin(); it2 != inherited_section.Data.end(); ++it2)
                        {
                            insert_item_ignore_duplicates(it->second, *it2);
                        }
                    }
                    else
                    {
                        Sect& inherited_section = *OLTXData[element[j]];
                        for (auto it2 = inherited_section.Data.begin(); it2 != inherited_section.Data.end(); ++it2)
                        {
                            insert_item_ignore_duplicates(it->second, *it2);
                        }
                    }
                }

                if (it->second)
                {
                    auto I = std::lower_bound(DATA.begin(), DATA.end(), *it->first, sect_pred);
                    if (I != DATA.end() && (*I)->Name == it->first)
                        xrDebug::Fatal(DEBUG_INFO, "Duplicate section '%s' found.", *it->first.c_str());
                    DATA.insert(I, it->second);
                }
            }
            else
            {
                auto I = std::lower_bound(DATA.begin(), DATA.end(), *it->first, sect_pred);
                if (I != DATA.end() && (*I)->Name == it->first)
                    xrDebug::Fatal(DEBUG_INFO, "Duplicate section '%s' found.", *it->first.c_str());
                DATA.insert(I, it->second);
            }
        }
    };
    
    std::function<void(IReader*, pcstr)> ModLoad = [&](IReader* R, pcstr path) 
    {
        R_ASSERT(R);
        Sect* ModCurrent = nullptr;
        string4096 modstr;
        string4096 modstr2;

        bool bInsideSTR = false;

        while (!R->eof())
        {
            R->r_string(modstr, sizeof modstr);
            _Trim(modstr);

            pstr comm = strchr(modstr, ';');
            pstr comm_1 = strchr(modstr, '/');

            if (comm_1 && *(comm_1 + 1) == '/' && (!comm || (comm && comm_1 < comm)))
            {
                comm = comm_1;
            }

#ifdef DEBUG
            pstr comment = 0;
#endif
            if (comm)
            {
                //."bla-bla-bla;nah-nah-nah"
                char quot = '"';
                bool in_quot = false;

                pcstr q1 = strchr(modstr, quot);
                if (q1 && q1 < comm)
                {
                    pcstr q2 = strchr(++q1, quot);
                    if (q2 && q2 > comm)
                        in_quot = true;
                }

                if (!in_quot)
                {
                    *comm = 0;
#ifdef DEBUG
                    comment = comm + 1;
#endif
                }
            }

            if (modstr[0] && modstr[0] == '!' && modstr[1] && modstr[1] == '[') // new section to overwrite?
            {
                if (strchr(modstr, ']') == nullptr)
                {
                    xrDebug::Fatal(DEBUG_INFO, "Wrong section '%s' detected in the mods for the file '%s'", modstr, m_file_name);
                }
                
                u32 SectionNameStartPos = 2;
                ModCurrent = xr_new<Sect>();
                ModCurrent->Name = nullptr;
                ModCurrent->Name = std::string(modstr)
                                       .substr(SectionNameStartPos, strchr(modstr, ']') - modstr - SectionNameStartPos)
                                       .c_str();
            }
            else if (modstr[0] && modstr[0] == '-' && modstr[1] && modstr[1] == '[') // new section to delete?
            {
                if (strchr(modstr, ']') == nullptr)
                {
                    xrDebug::Fatal(DEBUG_INFO, "Wrong section '%s' detected in the mods for the file '%s'", modstr, m_file_name);
                }
                
                u32 SectionNameStartPos = 2;

                DeleteSec.push_back(std::string(modstr)
                                        .substr(SectionNameStartPos, strchr(modstr, ']') - modstr - SectionNameStartPos)
                                        .c_str());
            }
            else if (modstr[0] && modstr[0] == '+' && modstr[1] && modstr[1] == '[') // new section to create?
            {
                if (strchr(modstr, ']') == nullptr)
                {
                    xrDebug::Fatal(DEBUG_INFO, "Wrong section '%s' detected in the mods for the file '%s'", modstr, m_file_name);
                }
                
                u32 SectionNameStartPos = 2;

                ModCurrent = xr_new<Sect>();
                ModCurrent->Name = nullptr;
                ModCurrent->Name = std::string(modstr)
                                       .substr(SectionNameStartPos, strchr(modstr, ']') - modstr - SectionNameStartPos)
                                       .c_str();
                CreateSec.push_back(ModCurrent->Name);

                pcstr inherited_names = strstr(modstr, "]:");
                if (nullptr != inherited_names)
                {
                    if (Parents.find(ModCurrent->Name) == Parents.end())
                    {
                        inherited_names += 2;
                        u32 cnt = _GetItemCount(inherited_names);
                        for (u32 k = 0; k < cnt; ++k)
                        {
                            string512 tmp;
                            pstr inherited_name = _GetItem(inherited_names, k, tmp);
                            Parents[ModCurrent->Name].push_back(inherited_name);
                        }
                    }
                    else
                    {
                        xrDebug::Fatal(DEBUG_INFO, "Duplicate section '%s' detected in the mods for the file '%s'", modstr, m_file_name);
                    }
                }
            }
            else // name = value
            {
                if ((ModCurrent != nullptr) && (ModCurrent->Name != nullptr))
                {
                    string4096 value_raw;
                    char* name = (char*)(modstr);
                    char* t = strchr(name, '=');
                    if (t)
                    {
                        xr_strcpy(value_raw, t);
                        bInsideSTR = _parse(modstr2, value_raw);
                        if (bInsideSTR) // multiline str value
                        {
                            string4096 prevStr;
                            xr_strcpy(prevStr, modstr2);

                            bool incorrectFormat = false;
                            const size_t prevPos = F->tell();
                            while (bInsideSTR)
                            {
                                xr_strcat(value_raw, sizeof value_raw, "\r\n");
                                string4096 str_add_raw;
                                F->r_string(str_add_raw, sizeof str_add_raw);

                                cpstr sectionNameTester = strchr(str_add_raw, '[');
                                if (sectionNameTester)
                                {
                                    if (strchr(sectionNameTester, ']'))
                                    {
                                        // That's a new section name! This is 100% error!
                                        incorrectFormat = true;
                                    }
                                }

                                if (!(xr_strlen(value_raw) + xr_strlen(str_add_raw) < sizeof value_raw) ||
                                    incorrectFormat)
                                {
                                    Msg("! Incorrect inifile format: section[%s], variable[%s]. Odd number of quotes "
                                        "(\") found, but "
                                        "should be even. Trimming it to the first new line.",
                                        ModCurrent->Name.c_str(), name);
                                    _Trim(prevStr, '\"');
                                    xr_strcpy(modstr2, prevStr);
                                    F->seek(prevPos);
                                    break;
                                }

                                xr_strcat(value_raw, sizeof value_raw, str_add_raw);
                                bInsideSTR = _parse(modstr2, value_raw);
                                if (bInsideSTR)
                                {
                                    if (is_empty_line_now(F))
                                        xr_strcat(value_raw, sizeof value_raw, "\r\n");
                                }
                            }
                        }
                    }
                    else
                    {
                        _Trim(name);
                        modstr2[0] = 0;
                    }

                    if (name[0] && modstr2[0])
                    {
                        auto field_name = strtok(modstr, "=");
                        auto value = strtok(NULL, "=");
                        _Trim(field_name);
                        _Trim(value);

                        if (field_name[0] == '*' && field_name[1])
                        {
                            field_name = field_name++;
                            std::pair<std::string, std::string> Field{field_name, OLTX_DELETE};
                            ModData[ModCurrent->Name].push_back(Field);
                        }
                        else
                        {
                            std::pair<std::string, std::string> Field{field_name, value};
                            ModData[ModCurrent->Name].push_back(Field);
                        }
                    }
                    else
                    {
                        xrDebug::Fatal(DEBUG_INFO, "Wrong field detected in the mods for the file '%s'", m_file_name);
                    }
                }
            }
        }
    };

    std::function<bool(std::string, std::string)> CheckForMods = [&](std::string FileName, std::string FilePath) 
    {
        FS_FileSet OLTX_Files; // set of files with mod prefix + FileName
        FS.file_list(OLTX_Files, FilePath.c_str(), FS_ListFiles, ("mod_" + FileName + "_*.ltx").c_str());
        if (!OLTX_Files.empty())
        {
            for (auto iter = OLTX_Files.begin(); iter != OLTX_Files.end(); ++iter)
            {
                std::string ModFileName = iter->name.c_str();
                std::string FullPath = FilePath + ModFileName;
                auto PathToMod = FullPath.c_str();

                IReader* I = FS.r_open(PathToMod);
                R_ASSERT3(I, "Can't open file:", ModFileName.c_str());

                ModLoad(I, PathToMod);

                FS.r_close(I);
            }
            return true;
        }
        else
        {
            Checked_files.push_back(FileName);
            return false;
        }
    };

    std::function<void(IReader*, pcstr, bool)> OLTXLoad = [&](IReader* F, pcstr path, bool StarLoad) 
    { 
        auto IncludeLoad = [&, OLTXLoad](const string_path _fn, const string_path inc_path, const string_path name, bool IncludeStarLoad) 
        {
            if (!allow_include_func || allow_include_func(_fn))
            {
                IReader* IR = FS.r_open(_fn);
#ifndef XR_PLATFORM_WINDOWS // XXX: replace with runtime check for case-sensitivity
                if (IR == nullptr)
                {
                    xr_fs_nostrlwr(name);
                    strconcat(_fn, inc_path, name);
                    IR = FS.r_open(_fn);
                }
#endif
                R_ASSERT3(IR, "Can't find include file:", name);

                OLTXLoad(IR, inc_path, IncludeStarLoad);

                FS.r_close(IR);
            }
        };

        Sect* Current = nullptr;
        string4096 str;
        string4096 str2;

        bool bInsideSTR = false;
        bool HasLoadedModFiles = false;
        
        if (m_file_name[0])
        {
            string4096 split_drive;
            string4096 split_dir;
            string4096 split_name;

            _splitpath_s(m_file_name, split_drive, sizeof split_drive, split_dir, sizeof split_dir, split_name,
                sizeof split_name, NULL, 0);

            std::string FilePath = std::string(split_drive) + std::string(split_dir);
            std::string FileName = split_name;

            auto iter = std::find(Checked_files.begin(), Checked_files.end(), FileName);
            if (iter == Checked_files.end())
            {
                HasLoadedModFiles = CheckForMods(FileName, FilePath);
            }
            else
            {
                HasLoadedModFiles = false;
            }
        }

        while (!F->eof())
        {
            F->r_string(str, sizeof str);
            _Trim(str);

            pstr comm = strchr(str, ';');
            pstr comm_1 = strchr(str, '/');

            if (comm_1 && *(comm_1 + 1) == '/' && (!comm || (comm && comm_1 < comm)))
            {
                comm = comm_1;
            }

#ifdef DEBUG
            pstr comment = 0;
#endif
            if (comm)
            {
                //."bla-bla-bla;nah-nah-nah"
                char quot = '"';
                bool in_quot = false;

                pcstr q1 = strchr(str, quot);
                if (q1 && q1 < comm)
                {
                    pcstr q2 = strchr(++q1, quot);
                    if (q2 && q2 > comm)
                        in_quot = true;
                }

                if (!in_quot)
                {
                    *comm = 0;
#ifdef DEBUG
                    comment = comm + 1;
#endif
                }
            }

            if (str[0] && str[0] == '#' && strstr(str, "#include")) // handle includes
            {
                string_path inc_name;
                R_ASSERT(path && path[0]);
                if (_GetItem(str, 1, inc_name, '"'))
                {
                    string_path fn, inc_path, folder, new_name, extension;
                    strconcat(sizeof fn, fn, path, inc_name);
                    _splitpath(fn, inc_path, folder, new_name, extension);
                    xr_strcat(inc_path, sizeof(inc_path), folder);
                    xr_strcat(new_name, sizeof(new_name), extension);

                    if (strstr(inc_name, "*.ltx"))
                    {
                        FS_FileSet fset;
                        FS.file_list(fset, inc_path, FS_ListFiles, new_name);

                        for (FS_FileSet::iterator it = fset.begin(); it != fset.end(); it++)
                        {
                            string_path _fn;
                            strconcat(sizeof _fn, _fn, inc_path, (it->name).c_str());

                            IncludeLoad(_fn, inc_path, (it->name).c_str(), true);
                        }
                        
                        StarIncluding();
                        FinalParents.clear();
                        OLTXData.clear();
                    }
                    else
                    {
                        IncludeLoad(fn, inc_path, inc_name, false);
                    }
                }
            }
            else if (str[0] && str[0] == '[') // new section ?
            {
                // insert previous filled section
                if (Current && std::find(DeleteSec.begin(), DeleteSec.end(), Current->Name) == DeleteSec.end())
                {
                    // store previous section
                    if (!StarLoad)
                    {
                        auto I = std::lower_bound(DATA.begin(), DATA.end(), Current->Name.c_str(), sect_pred);
                        if (I != DATA.end() && (*I)->Name == Current->Name)
                        {
                            xrDebug::Fatal(DEBUG_INFO, "Duplicate section '%s' is added.", Current->Name.c_str());
                        }
                        DATA.insert(I, Current);
                    }
                    else
                    {
                        StoreSection(Current);
                    }  
                }

                Current = xr_new<Sect>();
                Current->Name = nullptr;
                // start new section
                R_ASSERT3(strchr(str, ']'), "Bad ini section found: ", str);
                
                pcstr inherited_names = strstr(str, "]:");
                if (nullptr != inherited_names)
                {
                    VERIFY2(m_flags.test(eReadOnly), "Allow for readonly mode only.");
                    inherited_names += 2;
                    u32 cnt = _GetItemCount(inherited_names);
                    u32 total_count = 0;
                    u32 k = 0;
                    
                    if (!StarLoad)
                    {
                        for (k = 0; k < cnt; ++k)
                        {
                            string512 tmp;
                            _GetItem(inherited_names, k, tmp);
                            Sect& inherited_section = r_section(tmp);
                            total_count += inherited_section.Data.size();
                        }

                        Current->Data.reserve(Current->Data.size() + total_count);

                        for (k = 0; k < cnt; ++k)
                        {
                            string512 tmp;
                            _GetItem(inherited_names, k, tmp);
                            Sect& inherited_section = r_section(tmp);
                            for (auto it = inherited_section.Data.begin(); it != inherited_section.Data.end(); ++it)
                                insert_item(Current, *it);
                        }
                    }
                    
                    *strchr(str, ']') = 0;
                    Current->Name = xr_strlwr(str + 1);

                    if (StarLoad)
                    {
                        for (k = 0; k < cnt; ++k)
                        {
                            string512 tmp;
                            _GetItem(inherited_names, k, tmp);
                            IncludeParents[Current->Name].push_back(tmp);
                        }
                    }
                }

                if (Current->Name == nullptr)
                {
                    *strchr(str, ']') = 0;
                    Current->Name = xr_strlwr(str + 1);
                }
            }
            else // name = value
            {
                if (Current)
                {
                    string4096 value_raw;
                    char* name = str;
                    char* t = strchr(name, '=');
                    if (t)
                    {
                        *t = 0;
                        _Trim(name);
                        ++t;
                        xr_strcpy(value_raw, t);
                        bInsideSTR = _parse(str2, value_raw);
                        if (bInsideSTR) // multiline str value
                        {
                            string4096 prevStr;
                            xr_strcpy(prevStr, str2);

                            bool incorrectFormat = false;
                            const size_t prevPos = F->tell();
                            while (bInsideSTR)
                            {
                                xr_strcat(value_raw, sizeof value_raw, "\r\n");
                                string4096 str_add_raw;
                                F->r_string(str_add_raw, sizeof str_add_raw);

                                cpstr sectionNameTester = strchr(str_add_raw, '[');
                                if (sectionNameTester)
                                {
                                    if (strchr(sectionNameTester, ']'))
                                    {
                                        // That's a new section name! This is 100% error!
                                        incorrectFormat = true;
                                    }
                                }

                                if (!(xr_strlen(value_raw) + xr_strlen(str_add_raw) < sizeof value_raw) ||
                                    incorrectFormat)
                                {
                                    Msg("! Incorrect inifile format: section[%s], variable[%s]. Odd number of quotes "
                                        "(\") found, but "
                                        "should be even. Trimming it to the first new line.",
                                        Current->Name.c_str(), name);
                                    _Trim(prevStr, '\"');
                                    xr_strcpy(str2, prevStr);
                                    F->seek(prevPos);
                                    break;
                                }

                                xr_strcat(value_raw, sizeof value_raw, str_add_raw);
                                bInsideSTR = _parse(str2, value_raw);
                                if (bInsideSTR)
                                {
                                    if (is_empty_line_now(F))
                                        xr_strcat(value_raw, sizeof value_raw, "\r\n");
                                }
                            }
                        }
                    }
                    else
                    {
                        _Trim(name);
                        str2[0] = 0;
                    }

                    Item I;

                    I.first = name[0] ? name : NULL;

                    if (!HasLoadedModFiles)
                    {
                        I.second = str2[0] ? str2 : NULL;
                    }
                    else
                    {
                        auto iter = std::find_if(ModData[Current->Name].begin(), ModData[Current->Name].end(),
                            [name](const auto& pair) { return pair.first == name; });

                        if (iter != ModData[Current->Name].end())
                        {
                            auto new_iter = *iter;
                            if (new_iter.second == OLTX_DELETE)
                            {
                                continue;
                            }
                            I.second = new_iter.second.c_str();
                        }
                        else
                        {
                            I.second = str2[0] ? str2 : NULL;
                        }
                    }

                    // #ifdef DEBUG
                    //  I.comment = m_flags.test(eReadOnly)?0:comment;
                    // #endif

                    if (m_flags.test(eReadOnly))
                    {
                        if (*I.first)
                            insert_item(Current, I);
                    }
                    else
                    {
                        if (*I.first || *I.second
                            // #ifdef DEBUG
                            //  || *I.comment
                            // #endif
                        )
                            insert_item(Current, I);
                    }
                }
            }
        }
        if (Current && std::find(DeleteSec.begin(), DeleteSec.end(), Current->Name) == DeleteSec.end())
        {
            if (!StarLoad)
            {
                auto I = std::lower_bound(DATA.begin(), DATA.end(), Current->Name.c_str(), sect_pred);
                if (I != DATA.end() && (*I)->Name == Current->Name)
                {
                    xrDebug::Fatal(DEBUG_INFO, "Duplicate section '%s' is added.", Current->Name.c_str());
                }
                DATA.insert(I, Current);
            }
            else
            {
                StoreSection(Current);
            }
        }
    };

    OLTXLoad(F, path, false);
    
    if (!CreateSec.empty())
    {
        for (int i = 0; i < CreateSec.size() - 1; i++)
        {
            Sect* Current = xr_new<Sect>();
            Current->Name = CreateSec[i];

            u32 total_count = 0;
            u32 k = 0;
            for (k = 0; k < Parents[Current->Name].size(); ++k)
            {
                total_count += ParentsLoad(total_count, Parents[Current->Name][k], CreateSec[i]);
            }

            Current->Data.reserve(Current->Data.size() + total_count);

            for (k = 0; k < FinalParents[Current->Name].size(); ++k)
            {
                Sect& inherited_section = r_section(FinalParents[Current->Name][k]);
                for (auto it = inherited_section.Data.begin(); it != inherited_section.Data.end(); ++it)
                {
                    insert_item(Current, *it);
                }
            }

            for (int j = 0; j < ModData[CreateSec[i]].size(); j++)
            {
                Item N;

                N.first = ModData[CreateSec[i]][j].first.c_str();
                N.second = ModData[CreateSec[i]][j].second.c_str();

                insert_item(Current, N);
            }
            auto I = std::lower_bound(DATA.begin(), DATA.end(), Current->Name.c_str(), sect_pred);
            if (I != DATA.end() && (*I)->Name == Current->Name)
            {
                xrDebug::Fatal(DEBUG_INFO, "Duplicate section '%s' is added by the CreationData.", Current->Name.c_str());
            }
            DATA.insert(I, Current);
        }
    }
}

void CInifile::save_as(IWriter& writer, bool bcheck) const
{
    string4096 temp, val;
    for (auto r_it = DATA.begin(); r_it != DATA.end(); ++r_it)
    {
        xr_sprintf(temp, sizeof temp, "[%s]", (*r_it)->Name.c_str());
        writer.w_string(temp);
        if (bcheck)
        {
            xr_sprintf(temp, sizeof temp, "; %d %d %d", (*r_it)->Name._get()->dwCRC, (*r_it)->Name._get()->dwReference,
                (*r_it)->Name._get()->dwLength);
            writer.w_string(temp);
        }

        for (auto s_it = (*r_it)->Data.begin(); s_it != (*r_it)->Data.end(); ++s_it)
        {
            const Item& I = *s_it;
            if (*I.first)
            {
                if (*I.second)
                {
                    _decorate(val, *I.second);
                    // only name and value
                    xr_sprintf(temp, sizeof temp, "%8s%-32s = %-32s", " ", I.first.c_str(), val);
                }
                else
                {
                    // only name
                    xr_sprintf(temp, sizeof temp, "%8s%-32s = ", " ", I.first.c_str());
                }
            }
            else
            {
                // no name, so no value
                temp[0] = 0;
            }
            _TrimRight(temp);
            if (temp[0])
                writer.w_string(temp);
        }
        writer.w_string(" ");
    }
}

bool CInifile::save_as(pcstr new_fname)
{
    // save if needed
    if (new_fname && new_fname[0])
        xr_strcpy(m_file_name, sizeof m_file_name, new_fname);

    R_ASSERT(m_file_name[0]);
    convert_path_separators(m_file_name);
    IWriter* F = FS.w_open_ex(m_file_name);
    if (!F)
        return false;

    save_as(*F);
    FS.w_close(F);
    return true;
}

bool CInifile::section_exist(pcstr S) const
{
    auto I = std::lower_bound(DATA.begin(), DATA.end(), S, sect_pred);
    return I != DATA.end() && xr_strcmp(*(*I)->Name, S) == 0;
}

bool CInifile::line_exist(pcstr S, pcstr L) const
{
    if (!section_exist(S))
        return false;
    Sect& I = r_section(S);
    auto A = std::lower_bound(I.Data.begin(), I.Data.end(), L, item_pred);
    return A != I.Data.end() && xr_strcmp(*A->first, L) == 0;
}

u32 CInifile::line_count(pcstr Sname) const
{
    Sect& S = r_section(Sname);
    u32 C = 0;
    for (const auto& item : S.Data)
        if (*item.first)
            C++;
    return C;
}

u32 CInifile::section_count() const { return DATA.size(); }
//--------------------------------------------------------------------------------------
CInifile::Sect& CInifile::r_section(const shared_str& S) const { return r_section(*S); }
bool CInifile::line_exist(const shared_str& S, const shared_str& L) const { return line_exist(*S, *L); }
u32 CInifile::line_count(const shared_str& S) const { return line_count(*S); }
bool CInifile::section_exist(const shared_str& S) const { return section_exist(*S); }
//--------------------------------------------------------------------------------------
// Read functions
//--------------------------------------------------------------------------------------
CInifile::Sect& CInifile::r_section(pcstr S) const
{
    char section[256];
    xr_strcpy(section, sizeof section, S);
    xr_strlwr(section);
    auto I = std::lower_bound(DATA.cbegin(), DATA.cend(), section, sect_pred);
    if (I == DATA.cend())
        xrDebug::Fatal(DEBUG_INFO, "Can't find section '%s'.", S);
    else if (xr_strcmp(*(*I)->Name, section))
    {
        // g_pStringContainer->verify();

        // string_path ini_dump_fn, path;
        // strconcat (sizeof(ini_dump_fn), ini_dump_fn, Core.ApplicationName, "_", Core.UserName, ".ini_log");
        //
        // FS.update_path (path, "$logs$", ini_dump_fn);
        // IWriter* F = FS.w_open_ex(path);
        // save_as (*F);
        // F->w_string ("shared strings:");
        // g_pStringContainer->dump(F);
        // FS.w_close (F);
        xrDebug::Fatal(DEBUG_INFO,
            "Can't open section '%s' (only '%s' avail). Please attach [*.ini_log] file to your bug report", section,
            *(*I)->Name);
    }
    return **I;
}

pcstr CInifile::r_string(pcstr S, pcstr L) const
{
    Sect const& I = r_section(S);
    auto A = std::lower_bound(I.Data.cbegin(), I.Data.cend(), L, item_pred);

    if (A != I.Data.cend() && xr_strcmp(*A->first, L) == 0)
        return *A->second;

    xrDebug::Fatal(DEBUG_INFO, "Can't find variable %s in [%s]", L, S);
    return nullptr;
}

shared_str CInifile::r_string_wb(pcstr S, pcstr L) const
{
    pcstr _base = r_string(S, L);

    if (_base == nullptr)
        return shared_str(nullptr);

    string4096 _original;
    xr_strcpy(_original, sizeof _original, _base);
    u32 _len = xr_strlen(_original);
    if (0 == _len)
        return shared_str("");
    if ('"' == _original[_len - 1])
        _original[_len - 1] = 0; // skip end
    if ('"' == _original[0])
        return shared_str(&_original[0] + 1); // skip begin
    return shared_str(_original);
}

u8 CInifile::r_u8(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    return u8(atoi(C));
}

u16 CInifile::r_u16(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    return u16(atoi(C));
}

u32 CInifile::r_u32(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    return u32(atoi(C));
}

u64 CInifile::r_u64(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
#ifndef _EDITOR
    return _strtoui64(C, nullptr, 10);
#else
    return (u64)_atoi64(C);
#endif
}

s64 CInifile::r_s64(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    return _atoi64(C);
}

s8 CInifile::r_s8(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    return s8(atoi(C));
}

s16 CInifile::r_s16(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    return s16(atoi(C));
}

s32 CInifile::r_s32(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    return s32(atoi(C));
}

float CInifile::r_float(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    return float(atof(C));
}

Fcolor CInifile::r_fcolor(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    Fcolor V = {0, 0, 0, 0};
    sscanf(C, "%f,%f,%f,%f", &V.r, &V.g, &V.b, &V.a);
    return V;
}

u32 CInifile::r_color(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    u32 r = 0, g = 0, b = 0, a = 255;
    sscanf(C, "%u,%u,%u,%u", &r, &g, &b, &a);
    return color_rgba(r, g, b, a);
}

Ivector2 CInifile::r_ivector2(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    Ivector2 V = {0, 0};
    sscanf(C, "%d,%d", &V.x, &V.y);
    return V;
}

Ivector3 CInifile::r_ivector3(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    Ivector V = {0, 0, 0};
    sscanf(C, "%d,%d,%d", &V.x, &V.y, &V.z);
    return V;
}

Ivector4 CInifile::r_ivector4(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    Ivector4 V = {0, 0, 0, 0};
    sscanf(C, "%d,%d,%d,%d", &V.x, &V.y, &V.z, &V.w);
    return V;
}

Fvector2 CInifile::r_fvector2(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    Fvector2 V = {0.f, 0.f};
    sscanf(C, "%f,%f", &V.x, &V.y);
    return V;
}

Fvector3 CInifile::r_fvector3(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    Fvector3 V = {0.f, 0.f, 0.f};
    sscanf(C, "%f,%f,%f", &V.x, &V.y, &V.z);
    return V;
}

Fvector4 CInifile::r_fvector4(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    Fvector4 V = {0.f, 0.f, 0.f, 0.f};
    sscanf(C, "%f,%f,%f,%f", &V.x, &V.y, &V.z, &V.w);
    return V;
}

bool CInifile::r_bool(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    VERIFY2(C && xr_strlen(C) <= 5, make_string("\"%s\" is not a valid bool value, section[%s], line[%s]", C, S, L));
    char B[8];
    xr_strcpy(B, 7, C);
    B[7] = 0;
    xr_strlwr(B);
    return isBool(B);
}

CLASS_ID CInifile::r_clsid(pcstr S, pcstr L) const
{
    pcstr C = r_string(S, L);
    return TEXT2CLSID(C);
}

int CInifile::r_token(pcstr S, pcstr L, const xr_token* token_list) const
{
    pcstr C = r_string(S, L);
    for (int i = 0; token_list[i].name; i++)
        if (!xr_stricmp(C, token_list[i].name))
            return token_list[i].id;
    return 0;
}

bool CInifile::r_line(pcstr S, int L, pcstr* N, pcstr* V) const
{
    Sect& SS = r_section(S);
    if (L >= (int)SS.Data.size() || L < 0)
        return false;
    for (auto I = SS.Data.cbegin(); I != SS.Data.cend(); ++I)
        if (!L--)
        {
            *N = *I->first;
            *V = *I->second;
            return true;
        }
    return false;
}

bool CInifile::r_line(const shared_str& S, int L, pcstr* N, pcstr* V) const { return r_line(*S, L, N, V); }
//--------------------------------------------------------------------------------------------------------
// Write functions
//--------------------------------------------------------------------------------------
void CInifile::w_string(pcstr S, pcstr L, pcstr V, pcstr comment)
{
    R_ASSERT(!m_flags.test(eReadOnly));

    // section
    string256 sect;
    _parse(sect, S);
    xr_strlwr(sect);

    if (!section_exist(sect))
    {
        // create _new_ section
        Sect* NEW = xr_new<Sect>();
        NEW->Name = sect;
        auto I = std::lower_bound(DATA.begin(), DATA.end(), sect, sect_pred);
        DATA.insert(I, NEW);
    }

    // parse line/value
    string4096 line;
    _parse(line, L);
    string4096 value;
    _parse(value, V);

    // duplicate & insert
    Item I;
    Sect& data = r_section(sect);
    I.first = line[0] ? line : 0;
    I.second = value[0] ? value : 0;

    // #ifdef DEBUG
    //  I.comment = (comment?comment:0);
    // #endif
    auto it = std::lower_bound(data.Data.begin(), data.Data.end(), *I.first, item_pred);

    if (it != data.Data.end())
    {
        // Check for "first" matching
        if (0 == xr_strcmp(*it->first, *I.first))
        {
            bool b = m_flags.test(eOverrideNames);
            R_ASSERT2(b, make_string("name[%s] already exist in section[%s]", line, sect).c_str());
            *it = I;
        }
        else
            data.Data.insert(it, I);
    }
    else
        data.Data.insert(it, I);
}
void CInifile::w_u8(pcstr S, pcstr L, u8 V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d", V);
    w_string(S, L, temp, comment);
}
void CInifile::w_u16(pcstr S, pcstr L, u16 V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d", V);
    w_string(S, L, temp, comment);
}
void CInifile::w_u32(pcstr S, pcstr L, u32 V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d", V);
    w_string(S, L, temp, comment);
}

void CInifile::w_u64(pcstr S, pcstr L, u64 V, pcstr comment)
{
    string128 temp;
#ifndef _EDITOR
    _ui64toa_s(V, temp, sizeof temp, 10);
#else
    _ui64toa(V, temp, 10);
#endif
    w_string(S, L, temp, comment);
}

void CInifile::w_s64(pcstr S, pcstr L, s64 V, pcstr comment)
{
    string128 temp;
#ifndef _EDITOR
    _i64toa_s(V, temp, sizeof temp, 10);
#else
    _i64toa(V, temp, 10);
#endif
    w_string(S, L, temp, comment);
}

void CInifile::w_s8(pcstr S, pcstr L, s8 V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d", V);
    w_string(S, L, temp, comment);
}
void CInifile::w_s16(pcstr S, pcstr L, s16 V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d", V);
    w_string(S, L, temp, comment);
}
void CInifile::w_s32(pcstr S, pcstr L, s32 V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d", V);
    w_string(S, L, temp, comment);
}
void CInifile::w_float(pcstr S, pcstr L, float V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%f", V);
    w_string(S, L, temp, comment);
}
void CInifile::w_fcolor(pcstr S, pcstr L, const Fcolor& V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%f,%f,%f,%f", V.r, V.g, V.b, V.a);
    w_string(S, L, temp, comment);
}

void CInifile::w_color(pcstr S, pcstr L, u32 V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d,%d,%d,%d", color_get_R(V), color_get_G(V), color_get_B(V), color_get_A(V));
    w_string(S, L, temp, comment);
}

void CInifile::w_ivector2(pcstr S, pcstr L, const Ivector2& V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d,%d", V.x, V.y);
    w_string(S, L, temp, comment);
}

void CInifile::w_ivector3(pcstr S, pcstr L, const Ivector3& V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d,%d,%d", V.x, V.y, V.z);
    w_string(S, L, temp, comment);
}

void CInifile::w_ivector4(pcstr S, pcstr L, const Ivector4& V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%d,%d,%d,%d", V.x, V.y, V.z, V.w);
    w_string(S, L, temp, comment);
}
void CInifile::w_fvector2(pcstr S, pcstr L, const Fvector2& V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%f,%f", V.x, V.y);
    w_string(S, L, temp, comment);
}

void CInifile::w_fvector3(pcstr S, pcstr L, const Fvector3& V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%f,%f,%f", V.x, V.y, V.z);
    w_string(S, L, temp, comment);
}

void CInifile::w_fvector4(pcstr S, pcstr L, const Fvector4& V, pcstr comment)
{
    string128 temp;
    xr_sprintf(temp, sizeof temp, "%f,%f,%f,%f", V.x, V.y, V.z, V.w);
    w_string(S, L, temp, comment);
}

void CInifile::w_bool(pcstr S, pcstr L, bool V, pcstr comment) { w_string(S, L, V ? "on" : "off", comment); }
void CInifile::remove_line(pcstr S, pcstr L)
{
    R_ASSERT(!m_flags.test(eReadOnly));

    if (line_exist(S, L))
    {
        Sect& data = r_section(S);
        auto A = std::lower_bound(data.Data.begin(), data.Data.end(), L, item_pred);
        R_ASSERT(A != data.Data.end() && xr_strcmp(*A->first, L) == 0);
        data.Data.erase(A);
    }
}

template <>
XRCORE_API pcstr CInifile::read(pcstr section, pcstr line) const
{
    return r_string(section, line);
}

template <>
XRCORE_API u8 CInifile::read(pcstr section, pcstr line) const
{
    return r_u8(section, line);
}

template <>
XRCORE_API u16 CInifile::read(pcstr section, pcstr line) const
{
    return r_u16(section, line);
}

template <>
XRCORE_API u32 CInifile::read(pcstr section, pcstr line) const
{
    return r_u32(section, line);
}

template <>
XRCORE_API s8 CInifile::read(pcstr section, pcstr line) const
{
    return r_s8(section, line);
}

template <>
XRCORE_API s16 CInifile::read(pcstr section, pcstr line) const
{
    return r_s16(section, line);
}

template <>
XRCORE_API s32 CInifile::read(pcstr section, pcstr line) const
{
    return r_s32(section, line);
}

template <>
XRCORE_API s64 CInifile::read(pcstr section, pcstr line) const
{
    return r_s64(section, line);
}

template <>
XRCORE_API float CInifile::read(pcstr section, pcstr line) const
{
    return r_float(section, line);
}

template <>
XRCORE_API Fcolor CInifile::read(pcstr section, pcstr line) const
{
    return r_fcolor(section, line);
}

template <>
XRCORE_API Ivector2 CInifile::read(pcstr section, pcstr line) const
{
    return r_ivector2(section, line);
}

template <>
XRCORE_API Ivector3 CInifile::read(pcstr section, pcstr line) const
{
    return r_ivector3(section, line);
}

template <>
XRCORE_API Ivector4 CInifile::read(pcstr section, pcstr line) const
{
    return r_ivector4(section, line);
}

template <>
XRCORE_API bool CInifile::try_read(Ivector4& outValue, pcstr section, pcstr line) const
{
    pcstr C = r_string(section, line);
    return 4 == sscanf(C, "%d,%d,%d,%d", &outValue.x, &outValue.y, &outValue.z, &outValue.w);
}

template <>
XRCORE_API Fvector2 CInifile::read(pcstr section, pcstr line) const
{
    return r_fvector2(section, line);
}

template <>
XRCORE_API bool CInifile::try_read(Fvector2& outValue, pcstr section, pcstr line) const
{
    pcstr C = r_string(section, line);
    return 2 == sscanf(C, "%f,%f", &outValue.x, &outValue.y);
}

template <>
XRCORE_API Fvector3 CInifile::read(pcstr section, pcstr line) const
{
    return r_fvector3(section, line);
}

template <>
XRCORE_API Fvector4 CInifile::read(pcstr section, pcstr line) const
{
    return r_fvector4(section, line);
}

template <>
XRCORE_API bool CInifile::read(pcstr section, pcstr line) const
{
    return r_bool(section, line);
}
