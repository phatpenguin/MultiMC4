// 
//  Copyright 2012 MultiMC Contributors
// 
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
// 
//        http://www.apache.org/licenses/LICENSE-2.0
// 
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//
#include "multimc_pragma.h"
#include "instance.h"
#include "stdinstance.h"

#include <wx/filesys.h>
#include <wx/sstream.h>
#include <wx/wfstream.h>
#include <wx/mstream.h>
#include <wx/dir.h>
#include <wx/zipstrm.h>
#include <wx/txtstrm.h>
#include <memory>
#include <sstream>

#include "utils/osutils.h"
#include "utils/datautils.h"
#include "insticonlist.h"
#include "java/javautils.h"
#include "instancemodel.h"
#include "mcprocess.h"
#include "mcversionlist.h"

const wxString cfgFileName = "instance.cfg";

bool IsValidInstance(wxFileName rootDir)
{
	return rootDir.DirExists() && wxFileExists(Path::Combine(rootDir, cfgFileName));
}

Instance *Instance::LoadInstance(wxString rootDir)
{
	if (IsValidInstance(wxFileName::DirName(rootDir)))
	{
		wxFileConfig fcfg(wxEmptyString, wxEmptyString, 
			Path::Combine(rootDir, "instance.cfg"), wxEmptyString,
			wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH);

		int type = 0; 
		if (!fcfg.Read("type", &type))
		{
			type = INST_TYPE_STANDARD;
		}

		switch (type)
		{
		case INST_TYPE_STANDARD:
		default:
			return new StdInstance(rootDir);
			break;
		}
	}
	else
		return NULL;
}

Instance::Instance(const wxString &rootDir)
	: modList(this), m_running(false)
{
	if (!rootDir.EndsWith("/"))
		this->rootDir = wxFileName::DirName(rootDir + "/");
	else
		this->rootDir = wxFileName::DirName(rootDir);
	config = new wxFileConfig(wxEmptyString, wxEmptyString, GetConfigPath().GetFullPath(), wxEmptyString,
		wxCONFIG_USE_LOCAL_FILE | wxCONFIG_USE_RELATIVE_PATH);
	MkDirs();

	// initialize empty mod lists - they are filled later and only if requested (see apropriate Get* methods)
	modList.SetDir(GetInstModsDir().GetFullPath());
	mlModList.SetDir(GetMLModsDir().GetFullPath());
	coreModList.SetDir(GetCoreModsDir().GetFullPath());
	worldList.SetDir(GetSavesDir().GetFullPath());
	tpList.SetDir(GetTexturePacksDir().GetFullPath());
	modloader_list_inited = false;
	coremod_list_inited = false;
	jar_list_inited = false;
	world_list_initialized = false;
	tp_list_initialized = false;
	parentModel = nullptr;
	UpdateVersion();
}

Instance::~Instance(void)
{
	delete config;
	Save();
}

void Instance::SetParentModel ( InstanceModel* parent )
{
	parentModel = parent;
}


void Instance::UpdateVersion ( bool keep_current )
{
	wxFileName jar = GetMCJar();
	if(!jar.FileExists())
	{
		SetJarTimestamp(0);
		SetJarVersion(MCVer_Unknown);
		return;
	}
	
	auto dt = jar.GetModificationTime();
	dt.MakeUTC(true);
	auto time =dt.GetTicks();
	if(keep_current)
	{
		SetJarTimestamp(time);
		return;
	}
	auto saved_time = GetJarTimestamp();
	if(time != saved_time)
	{
		wxString newversion = javautils::GetMinecraftJarVersion(jar);
		SetJarTimestamp(time);
		SetJarVersion(newversion);
	}
}


// Makes ALL the directories! \o/
void Instance::MkDirs()
{
	if (!GetRootDir().DirExists())
		GetRootDir().Mkdir();
	
	if (!GetMCDir().DirExists())
		GetMCDir().Mkdir();
	if (!GetBinDir().DirExists())
		GetBinDir().Mkdir();
	if (!GetSavesDir().DirExists())
		GetSavesDir().Mkdir();
	if (!GetMLModsDir().DirExists())
		GetMLModsDir().Mkdir();
	if (!GetCoreModsDir().DirExists())
		GetCoreModsDir().Mkdir();
	if (!GetResourceDir().DirExists())
		GetResourceDir().Mkdir();
	if (!GetScreenshotsDir().DirExists())
		GetScreenshotsDir().Mkdir();
	if (!GetTexturePacksDir().DirExists())
		GetTexturePacksDir().Mkdir();
	
	if (!GetInstModsDir().DirExists())
		GetInstModsDir().Mkdir();
}

bool Instance::Save() const
{
	return false;
}

bool Instance::Load(bool loadDefaults)
{
	return false;
}

wxFileName Instance::GetRootDir() const
{
	return rootDir;
}

wxFileName Instance::GetConfigPath() const
{
	return wxFileName(rootDir.GetFullPath(), cfgFileName);
}

wxFileName Instance::GetMCDir() const
{
	wxFileName mcDir;

	wxString dotMCDir = Path::Combine(GetRootDir(), ".minecraft");
	wxString nodotMCDir = Path::Combine(GetRootDir(), "minecraft");
	
	if (wxDirExists(dotMCDir) && !wxDirExists(nodotMCDir))
	{
		mcDir = wxFileName::DirName(dotMCDir);
	}
	else
	{
		mcDir = wxFileName::DirName(nodotMCDir);
	}
	
	return mcDir;
}

wxFileName Instance::GetBinDir() const
{
	return wxFileName::DirName(GetMCDir().GetFullPath() + "/bin");
}

wxFileName Instance::GetCoreModsDir() const
{
	return wxFileName::DirName(Path::Combine(GetMCDir().GetFullPath(), "coremods"));
}

wxFileName Instance::GetMLModsDir() const
{
	return wxFileName::DirName(Path::Combine(GetMCDir().GetFullPath(), "mods"));
}

wxFileName Instance::GetResourceDir() const
{
	return wxFileName::DirName(Path::Combine(GetMCDir().GetFullPath(), "resources"));
}

wxFileName Instance::GetSavesDir() const
{
	return wxFileName::DirName(Path::Combine(GetMCDir().GetFullPath(), "saves"));
}

wxFileName Instance::GetScreenshotsDir() const
{
	return wxFileName::DirName(Path::Combine(GetMCDir().GetFullPath(), "screenshots"));
}

wxFileName Instance::GetTexturePacksDir() const
{
	return wxFileName::DirName(Path::Combine(GetMCDir().GetFullPath(), "texturepacks"));
}


wxFileName Instance::GetInstModsDir() const
{
	return wxFileName::DirName(Path::Combine(GetRootDir().GetFullPath(), "instMods"));
}

wxFileName Instance::GetVersionFile() const
{
	return wxFileName::FileName(GetBinDir().GetFullPath() + "/version");
}

wxFileName Instance::GetMCBackup() const
{
	return wxFileName::FileName(GetBinDir().GetFullPath() + "/mcbackup.jar");
}

wxFileName Instance::GetMCJar() const
{
	return wxFileName::FileName(GetBinDir().GetFullPath() + "/minecraft.jar");
}

wxFileName Instance::GetModListFile() const
{
	return wxFileName::FileName(Path::Combine(GetRootDir(), "modlist"));
}


int64_t Instance::ReadVersionFile()
{
	int64_t number = -1;
	if (!GetVersionFile().FileExists()) return number;
	
	// Open the file for reading
	wxFFileInputStream input( GetVersionFile().GetFullPath() );
	if(input.IsOk())
	{
		auto len = input.GetLength();
		char * buf = new char[len];
		input.Read(buf, len);
		std::istringstream in(buf);
		in >> number;
		delete[] buf;
	}
	return number;
}

void Instance::WriteVersionFile(int64_t number)
{
	if (!GetBinDir().DirExists())
		GetBinDir().Mkdir();
	
	wxFile vFile;
	if (!vFile.Create(GetVersionFile().GetFullPath(), true))
		return;
	wxFileOutputStream outStream(vFile);
	wxString contents;
	contents << number;
	wxStringInputStream inStream(contents);
	outStream.Write(inStream);
}


wxString Instance::GetName() const
{
	return GetSetting<wxString>("name", _("Unnamed Instance"));
}

void Instance::SetName(wxString name)
{
	SetSetting<wxString>("name", name);
	if(parentModel)
		parentModel->InstanceRenamed(this);
}

wxString Instance::GetIconKey() const
{
	wxString iconKey = GetSetting<wxString>("iconKey", "default");
	return InstIconList::getRealIconKeyForEasterEgg(iconKey,GetName());
}

void Instance::SetIconKey(wxString iconKey)
{
	SetSetting<wxString>("iconKey", iconKey);
}

wxString Instance::GetNotes() const
{
	return GetSetting<wxString>("notes", wxEmptyString);
}

void Instance::SetNotes(wxString notes)
{
	SetSetting<wxString>("notes", notes);
}

bool Instance::ShouldRebuild() const
{
	return GetSetting<bool>("NeedsRebuild", false);
}

void Instance::SetNeedsRebuild(bool value)
{
	SetSetting<bool>("NeedsRebuild", value);
}

bool Instance::HasMCJar()
{
	return GetMCJar().FileExists();
}

bool Instance::HasMCBackup()
{
	return GetMCBackup().FileExists();
}

ModList *Instance::GetModList()
{
	// if nothing requested the jar list yet, load it.
	if(!jar_list_inited)
	{
		// load jar mod list from file, update it and save the list to file again
		modList.LoadFromFile(GetModListFile().GetFullPath());
		modList.UpdateModList();
		modList.SaveToFile(GetModListFile().GetFullPath());
		jar_list_inited = true;
	}
	return &modList;
}

ModList *Instance::GetMLModList()
{
	// if nothing requested the modloader list yet, load it.
	if(!modloader_list_inited)
	{
		// check the modloader mods...
		mlModList.UpdateModList();
		modloader_list_inited = true;
	}
	return &mlModList;
}

ModList *Instance::GetCoreModList()
{
	// if nothing requested the modloader list yet, load it.
	if(!coremod_list_inited)
	{
		// check the modloader mods...
		coreModList.UpdateModList();
		coremod_list_inited = true;
	}
	return &coreModList;
}

WorldList *Instance::GetWorldList()
{
	if (!world_list_initialized)
	{
		worldList.UpdateWorldList();
		world_list_initialized = true;
	}
	return &worldList;
}

TexturePackList *Instance::GetTexturePackList()
{
	if (!tp_list_initialized)
	{
		tpList.UpdateTexturePackList();
		tp_list_initialized = true;
	}
	return &tpList;
}

void Instance::GetPossibleConfigFiles(wxArrayString *array, wxString dir)
{
	if (dir.IsEmpty())
		dir = GetMCDir().GetFullPath();
	
	wxDir mcDir(dir);

	wxString currentFile;
	if (mcDir.GetFirst(&currentFile))
	{
		do
		{
			wxFileName fileName(Path::Combine(dir, currentFile));
			
			if (wxDirExists(fileName.GetFullPath()))
			{
				if (currentFile != "saves")
					GetPossibleConfigFiles(array, fileName.GetFullPath());
			}
			else if (fileName.GetExt() == "cfg" || 
				fileName.GetExt() == "conf" || 
				fileName.GetExt() == "config" || 
				fileName.GetExt() == "props" || 
				fileName.GetExt() == "properties" ||
				fileName.GetExt() == "xml" ||
				fileName.GetExt() == "yml" ||
				fileName.GetFullPath().Contains("options") ||
				fileName.GetPath().Contains("config") ||
				fileName.GetName().Contains("config") ||
				fileName.GetName().Contains("options"))
			{
				fileName.MakeRelativeTo(GetRootDir().GetFullPath());
				array->Add(fileName.GetFullPath());
			}
		} while (mcDir.GetNext(&currentFile));
	}
}

wxString Instance::GetInstID() const
{
	wxString id = GetRootDir().GetDirs()[GetRootDir().GetDirCount() - 1];
	return id;
}

wxString Instance::GetGroup()
{
	InstanceGroup *group = parentModel->GetInstanceGroup(this);
	if (group)
		return group->GetName();
	else
		return wxEmptyString;
}

void Instance::SetGroup ( const wxString& group )
{
	parentModel->SetInstanceGroup(this, group);
}
