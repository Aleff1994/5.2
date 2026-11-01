#ifndef _TextClien_H
#define _TextClien_H

#include <unordered_map>
#include <string>

class cTextClien
{
public:
	void Load();
	void BuildUILabelTranslations();
	const char* TranslateUILabel(const char* text) const;
	std::string LocalizeItemName(const std::string& name) const;
	std::unordered_map<std::string, std::string> m_UILabels;
	const char* Select(const char* por, const char* eng, const char* spn) const
	{ return m_Language == 1 ? eng : (m_Language == 2 ? spn : por); }
	int m_Language = 0;

	char TextClien_ThongTin[50][255];
	char TextClien_Khac[50][255];
	char TextClien_ClassName[52][255];
	char TextClien_Options[40][255];

};
//===================================================

#define MENU_GAME_FILE "./Data/Custom/TextClien/TextClien.ini"


extern cTextClien gTextClien;

#endif
