#include "StdAfx.h"
#include "TextClien.h"
#include "GlobalText.h"
#include "ZzzInfomation.h"
#include <set>
#include "Util.h"
#include <stdlib.h>
#include "windows.h"
//#include "atlstr.h"
#include "Interface.h"
#include "stdio.h"
#include "./Utilities/Log/muConsoleDebug.h"

#include "zzzinventory.h"
#include "dsplaysound.h"
#include "CSItemOption.h"

cTextClien gTextClien;

void cTextClien::Load()
{
	m_Language = _stricmp(g_strSelectedML.c_str(), "Eng") == 0 ? 1 : (_stricmp(g_strSelectedML.c_str(), "Spn") == 0 ? 2 : 0);
	char strFileName[255];
	memset(strFileName, 0, sizeof(strFileName));
	sprintf(strFileName, "./Data/Custom/TextClien/TextClien_%s.ini", g_strSelectedML.c_str());

	// Custom texts follow the language selected by script/registry. If a
	// translation is not installed yet, use English instead of displaying
	// the literal word "Null" throughout the interface.
	if (GetFileAttributesA(strFileName) == INVALID_FILE_ATTRIBUTES)
	{
		sprintf(strFileName, "./Data/Custom/TextClien/TextClien_Eng.ini");
	}

	char GetFotText[35] = { 0 };
	for (int st = 0; st < 50; st++)
	{
		wsprintf(GetFotText, "Mess%d", st);
		GetPrivateProfileStringA("ThongTin", GetFotText, "", TextClien_ThongTin[st], sizeof(TextClien_ThongTin[st]), strFileName);
		GetPrivateProfileStringA("Khac",   GetFotText, "", TextClien_Khac[st], sizeof(TextClien_Khac[st]), strFileName);
		if (st < 50)
		{
			GetPrivateProfileStringA("ClassName", GetFotText, "", TextClien_ClassName[st], sizeof(TextClien_ClassName[st]), strFileName);
			if (st < 40)
				GetPrivateProfileStringA("Options", GetFotText, "", TextClien_Options[st], sizeof(TextClien_Options[st]), strFileName);
		}

		g_ConsoleDebug->Write(5, "[Load text clien: %d: %s]", st, TextClien_ThongTin[st]);
	}

	for (int st = 50; st < 52; st++)
	{
		wsprintf(GetFotText, "Mess%d", st);
		GetPrivateProfileStringA("ClassName", GetFotText, "", TextClien_ClassName[st], sizeof(TextClien_ClassName[st]), strFileName);
	}

	


	BuildUILabelTranslations();
}

void cTextClien::BuildUILabelTranslations()
{
    // Only UI captions use this bank. Player names and player chat never do.
    m_UILabels.clear();
    std::set<std::string> ambiguous;
    const char* languages[] = { "Por", "Eng", "Spn" };
    for (const char* filename : { "Text.bmd", "GlobalText.bmd" })
    {
        CGlobalText banks[3];
        for (int i = 0; i < 3; ++i)
        {
            const std::string path = std::string("Data\\Local\\") + languages[i] + "\\" + filename;
            banks[i].Load(path, CGlobalText::LD_USA_CANADA_TEXTS | CGlobalText::LD_FOREIGN_TEXTS);
        }
        for (int i = 0; i < 3; ++i)
            for (const auto& source : banks[i].Entries())
            {
                const char* target = banks[m_Language].Get(source.first);
                if (source.second.empty() || !target || !*target || ambiguous.count(source.second)) continue;
                auto old = m_UILabels.find(source.second);
                if (old != m_UILabels.end() && old->second != target)
                {
                    m_UILabels.erase(old);
                    ambiguous.insert(source.second);
                }
                else m_UILabels[source.second] = target;
            }
    }

    std::vector<BYTE> skillBanks[3];
    for (int i = 0; i < 3; ++i)
        ReadLocalizedTable((std::string("Data\\Local\\") + languages[i] + "\\Skill.bmd").c_str(),
            skillBanks[i], MAX_SKILLS, sizeof(SKILL_ATTRIBUTE), 0x5A18);
    if (skillBanks[1].size() == MAX_SKILLS * sizeof(SKILL_ATTRIBUTE) &&
        skillBanks[m_Language].size() == skillBanks[1].size())
        for (int i = 0; i < MAX_SKILLS; ++i)
        {
            SKILL_ATTRIBUTE english, selected;
            memcpy(&english, skillBanks[1].data() + i * sizeof(english), sizeof(english));
            memcpy(&selected, skillBanks[m_Language].data() + i * sizeof(selected), sizeof(selected));
            english.Name[31] = selected.Name[31] = 0;
            if (english.Name[0] && selected.Name[0] && !m_UILabels.count(english.Name))
                m_UILabels[english.Name] = selected.Name;
        }
}

const char* cTextClien::TranslateUILabel(const char* text) const
{
    if (!text || !*text) return text;
    const auto found = m_UILabels.find(text);
    return found == m_UILabels.end() ? text : found->second.c_str();
}

std::string cTextClien::LocalizeItemName(const std::string& name) const
{
    if (m_Language == 1 || name.empty()) return name;
    const char* bankName = TranslateUILabel(name.c_str());
    if (bankName && name != bankName) return bankName;
    struct Name { const char* english; const char* portuguese; const char* spanish; };
    static const Name exact[] = {
        { "Short Sword", "Espada Curta", "Espada Corta" },
        { "Sword of Assassin", "Espada do Assassino", "Espada del Asesino" },
        { "Sword of Destruction", "Espada da Destruicao", "Espada de Destruccion" },
        { "Double Blade", "Lamina Dupla", "Hoja Doble" },
        { "Small Axe", "Machado Pequeno", "Hacha Pequena" },
        { "Hand Axe", "Machado de Mao", "Hacha de Mano" },
        { "Double Axe", "Machado Duplo", "Hacha Doble" },
        { "Battle Axe", "Machado de Batalha", "Hacha de Batalla" },
        { "Great Hammer", "Grande Martelo", "Gran Martillo" },
        { "Small Shield", "Escudo Pequeno", "Escudo Pequeno" },
        { "Large Shield", "Escudo Grande", "Escudo Grande" },
        { "Jewel of Bless", "Joia da Bencao", "Joya de Bendicion" },
        { "Jewel of Soul", "Joia da Alma", "Joya del Alma" },
        { "Jewel of Life", "Joia da Vida", "Joya de la Vida" },
        { "Jewel of Chaos", "Joia do Caos", "Joya del Caos" },
        { "Jewel of Creation", "Joia da Criacao", "Joya de Creacion" },
        { "Jewel of Harmony", "Joia da Harmonia", "Joya de Armonia" },
        { "Ring of Ice", "Anel de Gelo", "Anillo de Hielo" },
        { "Ring of Poison", "Anel de Veneno", "Anillo de Veneno" },
        { "Ring of Fire", "Anel de Fogo", "Anillo de Fuego" },
        { "Pendant of Lightning", "Pingente de Relampago", "Colgante de Rayo" },
        { "Pendant of Fire", "Pingente de Fogo", "Colgante de Fuego" }
    };
    for (const auto& row : exact)
        if (name == row.english) return Select(row.portuguese, row.english, row.spanish);
    static const Name nouns[] = {
        { "Helm", "Elmo", "Casco" }, { "Helmet", "Elmo", "Casco" },
        { "Armor", "Armadura", "Armadura" }, { "Pants", "Calcas", "Pantalones" },
        { "Gloves", "Luvas", "Guantes" }, { "Boots", "Botas", "Botas" },
        { "Sword", "Espada", "Espada" }, { "Blade", "Lamina", "Hoja" },
        { "Axe", "Machado", "Hacha" }, { "Spear", "Lanca", "Lanza" },
        { "Lance", "Lanca", "Lanza" }, { "Bow", "Arco", "Arco" },
        { "Crossbow", "Besta", "Ballesta" }, { "Staff", "Cajado", "Baston" },
        { "Scepter", "Cetro", "Cetro" }, { "Shield", "Escudo", "Escudo" },
        { "Ring", "Anel", "Anillo" }, { "Pendant", "Pingente", "Colgante" },
        { "Wings", "Asas", "Alas" }, { "Wing", "Asa", "Ala" },
        { "Orb", "Orbe", "Orbe" }, { "Scroll", "Pergaminho", "Pergamino" },
        { "Parchment", "Pergaminho", "Pergamino" }, { "Potion", "Pocao", "Pocion" },
        { "Jewel", "Joia", "Joya" }, { "Box", "Caixa", "Caja" },
        { "Cloak", "Capa", "Capa" }, { "Ticket", "Convite", "Entrada" },
        { "Fragment", "Fragmento", "Fragmento" }, { "Capsule", "Capsula", "Capsula" },
        { "Book", "Livro", "Libro" }, { "Mace", "Maca", "Maza" },
        { "Hammer", "Martelo", "Martillo" }, { "Scythe", "Foice", "Guadana" },
        { "Quiver", "Aljava", "Carcaj" }, { "Claws", "Garras", "Garras" },
        { "Sphere", "Esfera", "Esfera" }, { "Gun", "Arma", "Arma" }
    };
    for (const auto& row : nouns)
    {
        const std::string noun = row.english;
        const std::string translated = Select(row.portuguese, row.english, row.spanish);
        if (name == noun) return translated;
        const std::string ofPrefix = noun + " of ";
        if (name.compare(0, ofPrefix.size(), ofPrefix) == 0)
            return translated + " de " + TranslateUILabel(name.substr(ofPrefix.size()).c_str());
        const std::string prefix = noun + " ";
        if (name.compare(0, prefix.size(), prefix) == 0)
            return translated + " " + TranslateUILabel(name.substr(prefix.size()).c_str());
        const std::string suffix = " " + noun;
        if (name.size() > suffix.size() && name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
            return translated + " " + TranslateUILabel(name.substr(0, name.size() - suffix.size()).c_str());
    }
    // Keep unique set/model names (Kris, Katana, Bloodangel...) intact.
    return name;
}
