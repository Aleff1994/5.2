///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include <setjmp.h>
#include "ZzzTexture.h"
#include "./Utilities/Log/ErrorReport.h"
#include "WSclient.h"
#include "DSPlaySound.h"
#include "Jpeglib.h"
#include "ProtocolSend.h"

CGlobalBitmap Bitmaps;

struct my_error_mgr
{
	struct jpeg_error_mgr pub;
	jmp_buf setjmp_buffer;
};

typedef struct my_error_mgr* my_error_ptr;

METHODDEF(void) my_error_exit(j_common_ptr cinfo)
{
	my_error_ptr myerr = (my_error_ptr)cinfo->err;
	(*cinfo->err->output_message) (cinfo);
	longjmp(myerr->setjmp_buffer, 1);
}

bool WriteJpeg(char* filename, int Width, int Height, unsigned char* Buffer, int quality)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;
	FILE* outfile;
	JSAMPROW row_pointer[1];
	int row_stride;

	cinfo.err = jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);

	if ((outfile = fopen(filename, "wb")) == NULL)
	{
		//fprintf(stderr, "can't open %s\n", filename);
		//exit(1);
		return FALSE;
	}
	jpeg_stdio_dest(&cinfo, outfile);

	cinfo.image_width = Width;
	cinfo.image_height = Height;
	cinfo.input_components = 3;
	cinfo.in_color_space = JCS_RGB;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, quality, TRUE);
	jpeg_start_compress(&cinfo, TRUE);
	row_stride = cinfo.image_width * 3;

	while (cinfo.next_scanline < cinfo.image_height)
	{
		row_pointer[0] = &Buffer[(Height - 1 - cinfo.next_scanline) * row_stride];
		(void)jpeg_write_scanlines(&cinfo, row_pointer, 1);
	}

	jpeg_finish_compress(&cinfo);
	fclose(outfile);
	jpeg_destroy_compress(&cinfo);
	return TRUE;
}

void SaveImage(int HeaderSize, char* Ext, char* filename, BYTE* PakBuffer, int Size)
{
	if (PakBuffer == NULL || Size == 0)
	{
		char OpenFileName[256];
		strcpy(OpenFileName, "Data2\\");
		strcat(OpenFileName, filename);
		FILE* fp = fopen(OpenFileName, "rb");
		if (fp == NULL)
		{
			return;
		}
		fseek(fp, 0, SEEK_END);
		Size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		PakBuffer = new unsigned char[Size];
		fread(PakBuffer, 1, Size, fp);
		fclose(fp);
	}

	char Header[24];
	memcpy(Header, PakBuffer, HeaderSize);

	char NewFileName[256];
	int iTextcnt = 0;
	for (int i = 0; i < (int)strlen(filename); i++)
	{
		iTextcnt = i;
		NewFileName[i] = filename[i];
		if (filename[i] == '.') break;
	}
	NewFileName[iTextcnt + 1] = NULL;
	strcat(NewFileName, Ext);
	char SaveFileName[256];
	strcpy(SaveFileName, "Data\\");
	strcat(SaveFileName, NewFileName);
	FILE* fp = fopen(SaveFileName, "wb");
	if (fp == NULL) return;
	fwrite(Header, 1, HeaderSize, fp);
	fwrite(PakBuffer, 1, Size, fp);
	fclose(fp);

	if (PakBuffer == NULL || Size == 0)
	{
		SAFE_DELETE_ARRAY(PakBuffer);
	}
}

bool OpenJpegBuffer(char* filename, float* BufferFloat)
{
	struct jpeg_decompress_struct cinfo;
	struct my_error_mgr jerr;
	FILE* infile;
	JSAMPARRAY buffer;
	int row_stride;

	char FileName[256];

	char NewFileName[256];
	int iTextcnt = 0;
	for (int i = 0; i < (int)strlen(filename); i++)
	{
		iTextcnt = i;
		NewFileName[i] = filename[i];
		if (filename[i] == '.') break;
	}
	NewFileName[iTextcnt + 1] = NULL;
	strcpy(FileName, "Data\\");
	strcat(FileName, NewFileName);
	strcat(FileName, "OZJ");

	if ((infile = fopen(FileName, "rb")) == NULL)
	{
		char Text[256];
		sprintf(Text, "%s - File not exist.", FileName);
		g_ErrorReport.Write(Text);
		g_ErrorReport.Write("\r\n");
		MessageBox(gwinhandle->GethWnd(), Text, NULL, MB_OK);
		SendMessage(gwinhandle->GethWnd(), WM_DESTROY, 0, 0);
		return false;
	}

	fseek(infile, 24, SEEK_SET);

	cinfo.err = jpeg_std_error(&jerr.pub);
	jerr.pub.error_exit = my_error_exit;
	if (setjmp(jerr.setjmp_buffer))
	{
		jpeg_destroy_decompress(&cinfo);
		fclose(infile);
		return false;
	}
	jpeg_create_decompress(&cinfo);

	jpeg_stdio_src(&cinfo, infile);

	(void)jpeg_read_header(&cinfo, TRUE);

	(void)jpeg_start_decompress(&cinfo);
	row_stride = cinfo.output_width * cinfo.output_components;
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);

	size_t buff_size = (cinfo.output_width * cinfo.output_height * cinfo.output_components);

	unsigned char* Buffer = new BYTE[buff_size];

	while (cinfo.output_scanline < cinfo.output_height)
	{
		(void)jpeg_read_scanlines(&cinfo, buffer, 1);
		memcpy(Buffer + (cinfo.output_height - cinfo.output_scanline) * row_stride, buffer[0], row_stride);
	}

	UINT Index = 0;

	if (Buffer != NULL)
	{
		for (unsigned int y = 0; y < cinfo.output_height; y++)
		{
			for (unsigned int x = 0; x < cinfo.output_width; x++)
			{
				if ((Index + 2) < buff_size)
				{
					BufferFloat[Index + 0] = (float)Buffer[Index + 0] / 255.f;
					BufferFloat[Index + 1] = (float)Buffer[Index + 1] / 255.f;
					BufferFloat[Index + 2] = (float)Buffer[Index + 2] / 255.f;
				}
				Index += 3;
			}
		}
	}
	SAFE_DELETE_ARRAY(Buffer);

	(void)jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	fclose(infile);
	return true;
}

bool LoadBitmap(const char* szFileName, GLuint uiTextureIndex, GLuint uiFilter, GLuint uiWrapMode, bool bCheck, bool bFullPath)
{
	// Optional/custom systems may request an image before a filename is
	// configured. Never turn an empty optional path into "Data\\" or show a
	// blocking error box during gameplay.
	if (szFileName == NULL || szFileName[0] == '\0')
	{
		return false;
	}

	char szFullPath[256] = { 0, };

	// Look & Feel 3/4 usam pacotes de interface isolados. O HUD inferior atual
	// permanece em Interface\HUD\Look-N e nunca passa por este redirecionamento.
	// Se a textura adaptada nao existir ou for incompativel, o carregamento
	// continua abaixo usando a interface compartilhada original.
	if (bFullPath == false && gmProtect != NULL && (gmProtect->LookAndFeel == 3 || gmProtect->LookAndFeel == 4))
	{
		const char* interfacePrefix = "Interface\\";
		const size_t prefixLength = strlen(interfacePrefix);
		const int lookAndFeel = gmProtect->LookAndFeel;
		const char* preservedSkillTextures[] =
		{
			"Interface\\newui_skill.jpg",
			"Interface\\newui_skill2.jpg",
			"Interface\\newui_non_skill.jpg",
			"Interface\\newui_non_skill2.jpg",
			"Interface\\newui_skillbox.jpg",
			"Interface\\newui_skillbox2.jpg",
			"Interface\\newui_command.jpg",
			"Interface\\newui_non_command.jpg",
			"Interface\\newui_skill3.jpg",
			"Interface\\newui_non_skill3.jpg",
			"Interface\\command.jpg",
			"Interface\\HUD\\Look-2\\Skill.jpg",
			"Interface\\HUD\\Look-2\\Skill2.jpg",
			"Interface\\HUD\\Look-2\\skillbox.jpg",
			"Interface\\HUD\\Look-2\\skillbox2.jpg",
			"Interface\\HUD\\Look-2\\command.jpg",
		};
		bool preserveSkillTexture = false;
		for (size_t index = 0; index < sizeof(preservedSkillTextures) / sizeof(preservedSkillTextures[0]); ++index)
		{
			if (_stricmp(szFileName, preservedSkillTextures[index]) == 0)
			{
				preserveSkillTexture = true;
				break;
			}
		}

		// Estas janelas permanecem no layout classico (Look 0) mesmo quando o
		// restante do cliente usa Look 4. As adaptacoes dessas telas usam
		// dimensoes diferentes das coordenadas originais e causam sobreposicao.
		const char* classicMasterPrefix = "Interface\\new_Master_";
		const bool preserveClassicWindowTexture =
			(_strnicmp(szFileName, classicMasterPrefix, strlen(classicMasterPrefix)) == 0) ||
			(_stricmp(szFileName, "Interface\\laf0_antilag_panel.tga") == 0);

		// Look 3 usa a interface nova em varias telas, mas o topo esquerdo
		// (nome do mapa, coordenadas e botoes do minimapa) deve permanecer no
		// padrao antigo para nao deformar/desalinhar como no pacote S19.
		const char* preservedLook3MiniMapTextures[] =
		{
			"Interface\\Minimap_positionA.tga",
			"Interface\\Minimap_positionB.tga",
			"Interface\\Minimap_positionC.tga",
			"Interface\\newui_position02.tga",
			"Interface\\newui_SW_Minimap_Bt_command.tga",
			"Interface\\newui_SW_Minimap_Bt_group.tga",
			"Interface\\newui_SW_MiniMap_Bt_clearness.jpg",
			"Interface\\newui_SW_Minimap_Frame.tga",
			"Interface\\newui_SW_Time_Frame.tga",
			"Interface\\mini_map_ui_cancel.tga",
			"Interface\\mini_map_ui_cha.tga",
			"Interface\\mini_map_ui_corner.tga",
			"Interface\\mini_map_ui_line.jpg",
			"Interface\\mini_map_ui_npc.tga",
			"Interface\\mini_map_ui_party.tga",
			"Interface\\mini_map_ui_portal.tga",
			"Interface\\Miniframe.tga",
		};
		bool preserveLook3MiniMapTexture = false;
		if (lookAndFeel == 3)
		{
			for (size_t index = 0; index < sizeof(preservedLook3MiniMapTextures) / sizeof(preservedLook3MiniMapTextures[0]); ++index)
			{
				if (_stricmp(szFileName, preservedLook3MiniMapTextures[index]) == 0)
				{
					preserveLook3MiniMapTexture = true;
					break;
				}
			}
		}

		if (_strnicmp(szFileName, interfacePrefix, prefixLength) == 0 &&
			!(lookAndFeel == 3 && _strnicmp(szFileName, "Interface\\HUD\\Look-3\\", 21) == 0) &&
			!(lookAndFeel == 3 && _strnicmp(szFileName, "Interface\\Look-3\\", 17) == 0) &&
			_strnicmp(szFileName, "Interface\\HUD\\Look-4\\", 21) != 0 &&
			_strnicmp(szFileName, "Interface\\Look-4\\", 17) != 0 &&
			(lookAndFeel != 3 || preserveLook3MiniMapTexture == false) &&
			(lookAndFeel != 4 || preserveSkillTexture == false) &&
			(lookAndFeel != 4 || preserveClassicWindowTexture == false))
		{
			char szLookPath[256] = { 0, };
			sprintf_s(szLookPath, "Data\\Interface\\Look-%d\\%s", lookAndFeel, szFileName + prefixLength);
			if (Bitmaps.LoadImage(uiTextureIndex, szLookPath, uiFilter, uiWrapMode))
			{
				// Shared windows use classic pixel coordinates for atlas frames.
				// Retain the HD texture, exposing its audited logical dimensions.
				// Explicit Look4 and HUD resources never enter this redirection.
				if (lookAndFeel == 4)
				{
					static const char* const scaledWindowAtlases[] =
					{
						"newui_Account_title.tga",
						"newui_btn_empty_big.tga",
						"newui_btn_empty_small.tga",
						"newui_btn_empty_very_small.tga",
						"newui_Bt_accept.tga",
						"newui_Bt_mix.tga",
						"newui_button_cancel.tga",
						"newui_button_close.tga",
						"newui_button_ok.tga",
						"newui_commamd04.tga",
						"newui_guildmakeeditbox.tga",
						"newui_guild_tab01.tga",
						"newui_guild_tab02.tga",
						"newui_guild_tab04.tga",
						"newui_item_box.tga",
						"newui_item_earring.tga",
						"newui_item_money2.tga",
						"newui_Message_03.tga",
						"newui_msgbox_bottom.tga",
						"newui_msgbox_middle.tga",
						"newui_msgbox_top.tga",
						"newui_myquest_Line.tga",
						"newui_option_back06(L).tga",
						"newui_option_back06(R).tga",
						"newui_option_check.tga",
						"newui_option_top.tga",
						"newui_option_volume01.tga",
						"newui_option_volume02.tga",
						"newui_scrollbar_down.tga",
						"newui_scrollbar_m.tga",
						"newui_scrollbar_up.tga",
						"newui_scroll_off.tga",
						"newui_scroll_on.tga",
						"Quest_Bt_cast.tga",
						"Quest_bt_L.tga",
						"Quest_Bt_open.tga",
						"Quest_bt_R.tga",
						"Quest_tab01.tga",
						"Quest_tab02.tga",
						"Quest_tab03.tga",
					};
					for (size_t i = 0; i < sizeof(scaledWindowAtlases) / sizeof(scaledWindowAtlases[0]); ++i)
					{
						if (_stricmp(szFileName + prefixLength, scaledWindowAtlases[i]) == 0)
						{
							BITMAP_t* bitmap = &Bitmaps[uiTextureIndex];
							// Subsequent loads retain the same bitmap and increment Ref.
							if (bitmap->Ref == 1)
							{
								bitmap->Width /= 4.0f;
								bitmap->Height /= 4.0f;
								bitmap->output_width /= 4;
								bitmap->output_height /= 4;
							}
							break;
						}
					}
				}
				return true;
			}
		}
	}

	if (bFullPath == true)
	{
		strcpy(szFullPath, szFileName);
	}
	else
	{
		strcpy(szFullPath, "Data\\");
		strcat(szFullPath, szFileName);
	}

	if (bCheck)
	{
		if (false == Bitmaps.LoadImage(uiTextureIndex, szFullPath, uiFilter, uiWrapMode))
		{
			char szErrorMsg[256] = { 0, };
			sprintf(szErrorMsg, "LoadBitmap Failed: %s", szFullPath);
			PopUpErrorCheckMsgBox(szErrorMsg, true);
			return false;
		}
		return true;
	}
	return Bitmaps.LoadImage(uiTextureIndex, szFullPath, uiFilter, uiWrapMode);
}

void DeleteBitmap(GLuint uiTextureIndex, bool bForce)
{
	Bitmaps.UnloadImage(uiTextureIndex, bForce);
}

void PopUpErrorCheckMsgBox(const char* szErrorMsg, bool bForceDestroy)
{
	char szMsg[1024] = { 0, };
	strcpy(szMsg, szErrorMsg);

	if (bForceDestroy)
	{
		MessageBox(gwinhandle->GethWnd(), szErrorMsg, "ErrorCheckBox", MB_OK | MB_ICONERROR);
	}
	else
	{
		int iResult = MessageBox(gwinhandle->GethWnd(), szMsg, "ErrorCheckBox", MB_YESNO | MB_ICONERROR);

		if (IDYES == iResult)
		{
			return;
		}
	}

	//SocketClient.Close();
	//KillGLWindow();
	//DestroySound();
	//DestroyWindow();
	//CloseMainExe();
	//ExitProcess(0);
}
