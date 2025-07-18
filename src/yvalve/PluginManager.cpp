/*
 *  The contents of this file are subject to the Initial
 *  Developer's Public License Version 1.0 (the "License");
 *  you may not use this file except in compliance with the
 *  License. You may obtain a copy of the License at
 *  http://www.ibphoenix.com/main.nfs?a=ibphoenix&page=ibp_idpl.
 *
 *  Software distributed under the License is distributed AS IS,
 *  WITHOUT WARRANTY OF ANY KIND, either express or implied.
 *  See the License for the specific language governing rights
 *  and limitations under the License.
 *
 *  The Original Code was created by Adriano dos Santos Fernandes
 *  for the ScratchBird Open Source RDBMS project.
 *
 *  Copyright (c) 2008 Adriano dos Santos Fernandes <adrianosf@uol.com.br>
 *  and all contributors signed below.
 *
 *  All Rights Reserved.
 *  Contributor(s): ______________________________________.
 */

#include "firebird.h"
#include "firebird/impl/consts_pub.h"
#include "iberror.h"
#include "../yvalve/PluginManager.h"
#include "../yvalve/MasterImplementation.h"

#include "firebird/impl/sqlda_pub.h"
#include "../yvalve/why_proto.h"

#include "../common/os/path_utils.h"
#include "../common/os/fbsyslog.h"
#include "../common/StatusArg.h"
#include "../common/isc_proto.h"
#include "../common/classes/fb_string.h"
#include "../common/classes/init.h"
#include "../common/classes/semaphore.h"
#include "../common/classes/RefMutex.h"
#include "../common/config/config.h"
#include "../common/config/config_file.h"
#include "../common/utils_proto.h"
#include "../common/ScanDir.h"
#include "../common/classes/GenericMap.h"
#include "../common/db_alias.h"
#include "../common/dllinst.h"
#include "../common/file_params.h"
#include "../common/status.h"

#include "../yvalve/config/os/config_root.h"


// register builtin plugins
#include "../remote/client/interface.h"

//#define DEBUG_PLUGINS

using namespace ScratchBird;

namespace
{
	void changeExtension(PathName& file, const char* newExt)
	{
		PathName::size_type p = file.rfind(PathUtils::dir_sep);
		if (p == PathName::npos)
		{
			p = 0;
		}
		p = file.find('.', p);
		if (p == PathName::npos)
		{
			file += '.';
		}
		else
		{
			file.erase(p + 1);
		}

		file += newExt;
	}

	// Holds a reference to plugins.conf file
	class StaticConfHolder
	{
	public:
		explicit StaticConfHolder(MemoryPool& p)
			: confFile(FB_NEW_POOL(p) ConfigFile(p,
				fb_utils::getPrefix(IConfigManager::DIR_CONF, "plugins.conf"), ConfigFile::HAS_SUB_CONF))
		{
		}

		ConfigFile* get()
		{
			return confFile;
		}

	private:
		RefPtr<ConfigFile> confFile;
	};
	InitInstance<StaticConfHolder> pluginsConf;

	RefPtr<ConfigFile> findInPluginsConf(const char* param, const char* pluginName)
	{
		ConfigFile* f = pluginsConf().get();
		if (f)
		{
			const ConfigFile::Parameter* plugPar = f->findParameter(param, pluginName);
			if (plugPar && plugPar->sub.hasData())
			{
				return plugPar->sub;
			}
		}

		return RefPtr<ConfigFile>(NULL);
	}

	bool flShutdown = false;

	class ConfigParameterAccess final :
		public RefCntIface<IConfigEntryImpl<ConfigParameterAccess, CheckStatusWrapper> >
	{
	public:
		ConfigParameterAccess(IReferenceCounted* c, const ConfigFile::Parameter* p) : cf(c), par(p) { }

		// IConfigEntry implementation
		const char* getName()
		{
			return par ? par->name.c_str() : NULL;
		}

		const char* getValue()
		{
			return par ? par->value.nullStr() : NULL;
		}

		FB_BOOLEAN getBoolValue()
		{
			return par ? par->asBoolean() : 0;
		}

		ISC_INT64 getIntValue()
		{
			return par ? par->asInteger() : 0;
		}

		IConfig* getSubConfig(CheckStatusWrapper* status);

	private:
		RefPtr<IReferenceCounted> cf;
		const ConfigFile::Parameter* par;
	};

	class ConfigAccess final :
		public RefCntIface<IConfigImpl<ConfigAccess, CheckStatusWrapper> >
	{
	public:
		ConfigAccess(RefPtr<ConfigFile> c) : confFile(c) { }

		// IConfig implementation
		IConfigEntry* find(CheckStatusWrapper* status, const char* name)
		{
			try
			{
				return confFile.hasData() ? newParam(confFile->findParameter(name)) : NULL;
			}
			catch (const ScratchBird::Exception& ex)
			{
				ex.stuffException(status);
			}
			return NULL;
		}

		IConfigEntry* findValue(CheckStatusWrapper* status, const char* name, const char* value)
		{
			try
			{
				return confFile.hasData() ? newParam(confFile->findParameter(name, value)) : NULL;
			}
			catch (const ScratchBird::Exception& ex)
			{
				ex.stuffException(status);
			}
			return NULL;
		}

		IConfigEntry* findPos(CheckStatusWrapper* status, const char* name, unsigned int n)
		{
			try
			{
				if (!confFile.hasData())
				{
					return NULL;
				}

				const ConfigFile::Parameters& p = confFile->getParameters();
				FB_SIZE_T pos;
				if (!p.find(name, pos))
				{
					return NULL;
				}

				if (n + pos >= p.getCount() || p[n + pos].name != name)
				{
					return NULL;
				}

				return newParam(&p[n + pos]);
			}
			catch (const ScratchBird::Exception& ex)
			{
				ex.stuffException(status);
			}
			return NULL;
		}

	private:
		RefPtr<ConfigFile> confFile;

		IConfigEntry* newParam(const ConfigFile::Parameter* p)
		{
			if (p)
			{
				IConfigEntry* rc = FB_NEW ConfigParameterAccess(this, p);
				rc->addRef();
				return rc;
			}

			return NULL;
		}
	};

	IConfig* ConfigParameterAccess::getSubConfig(CheckStatusWrapper* status)
	{
		try
		{
			if (par && par->sub.hasData())
			{
				IConfig* rc = FB_NEW ConfigAccess(par->sub);
				rc->addRef();
				return rc;
			}
		}
		catch (const ScratchBird::Exception& ex)
		{
			ex.stuffException(status);
		}

		return NULL;
	}

	IConfig* findPluginConfig(ConfigFile* pluginLoaderConfig, const PathName& confName)
	{
		FbLocalStatus ls;

		if (pluginLoaderConfig)
		{
			const ConfigFile::Parameter* p = pluginLoaderConfig->findParameter("Config");

			if (p)
			{
				RefPtr<ConfigFile> configSection(findInPluginsConf("Config", p->value.c_str()));

				if (configSection.hasData())
				{
					IConfig* rc = FB_NEW ConfigAccess(configSection);
					rc->addRef();
					return rc;
				}
			}
		}

		IConfig* rc = PluginManagerInterfacePtr()->getConfig(&ls, confName.nullStr());
		check(&ls);
		return rc;
	}


	// Plugins registered when loading plugin module.
	struct RegisteredPlugin
	{
		RegisteredPlugin(IPluginFactory* f, const char* nm, unsigned int t)
			: factory(f), name(nm), type(t)
		{ }

		RegisteredPlugin(MemoryPool& p, IPluginFactory* f, const char* nm, unsigned int t)
			: factory(f), name(p, nm), type(t)
		{ }

		RegisteredPlugin()
			: factory(NULL), name(), type(0)
		{ }

		RegisteredPlugin(MemoryPool& p)
			: factory(NULL), name(p), type(0)
		{ }

		RegisteredPlugin(MemoryPool& p, const RegisteredPlugin& from)
			: factory(from.factory), name(p, from.name), type(from.type)
		{ }

		IPluginFactory* factory;
		PathName name;
		unsigned int type;
	};

	// Controls module, containing plugins.
	class PluginModule : public ScratchBird::RefCounted, public GlobalStorage
	{
	public:
		PluginModule(ModuleLoader::Module* pmodule, const PathName& pname);

		unsigned int addPlugin(const RegisteredPlugin& p)
		{
			regPlugins.add(p);
			return regPlugins.getCount() - 1;
		}

		int findPlugin(unsigned int type, const PathName& name)
		{
			// typically modules do not contain too many plugins
			// therefore direct array scan is OK here
			for (unsigned int i = 0; i < regPlugins.getCount(); ++i)
			{
				if (type == regPlugins[i].type && name == regPlugins[i].name)
				{
					return i;
				}
			}

			return -1;
		}

		RegisteredPlugin& getPlugin(unsigned int i)
		{
			return regPlugins[i];
		}

		PluginModule* findModule(const PathName& pname)
		{
			if (name == pname)
			{
				return this;
			}
			if (next)
			{
				return next->findModule(pname);
			}
			return NULL;
		}

		const char* getName() const noexcept
		{
			return name.nullStr();
		}

		void setCleanup(IPluginModule* c)
		{
			cleanup = c;
		}

		void resetCleanup(IPluginModule* c)
		{
			if (cleanup == c)
			{
				cleanup = 0;
#ifdef DEBUG_PLUGINS
				fprintf(stderr, "resetCleanup() of module %s\n", name.c_str());
#endif
				// This is called only by unregister module
				// when current module is forced to go away by OS.
				// Do not unload it ourselves in this case.
				addRef();
			}
			else if (next)
			{
				next->resetCleanup(c);
			}
			else
			{
				gds__log("Failed to reset cleanup %p\n", c);
			}
		}

		void threadDetach()
		{
			if (cleanup)
				cleanup->threadDetach();
			if (next)
				next->threadDetach();
		}

	private:
		~PluginModule()
		{
			if (next)
			{
				next->prev = prev;
			}
			*prev = next;

			if (cleanup)
			{
				// Pause timer thread for cleanup period
				MutexLockGuard timerPause(Why::pauseTimer(), FB_FUNCTION);

				cleanup->doClean();
			}
		}

		PathName name;
		ScratchBird::AutoPtr<ModuleLoader::Module> module;
		ScratchBird::IPluginModule* cleanup;
		ObjectsArray<RegisteredPlugin> regPlugins;
		PluginModule* next;
		PluginModule** prev;
	};

	struct CountByType
	{
		int counter;
		Semaphore* waitsOn;

		CountByType()
			: counter(0), waitsOn(NULL)
		{ }
	};

	struct CountByTypeArray
	{
		explicit CountByTypeArray(MemoryPool&)
		{}

		CountByType values[PluginManager::TYPE_COUNT];

		CountByType& get(unsigned int t)
		{
			fb_assert(t < PluginManager::TYPE_COUNT);
			return values[t];
		}
	};

	GlobalPtr<CountByTypeArray> byTypeCounters;

	PluginModule* builtin = NULL;

	// Provides most of configuration services for plugins,
	// except per-database configuration in databases.conf
	class ConfiguredPlugin final :
		public RefCntIface<ITimerImpl<ConfiguredPlugin, CheckStatusWrapper> >
	{
	public:
		ConfiguredPlugin(RefPtr<PluginModule> pmodule, unsigned int preg,
						 RefPtr<ConfigFile> pconfig, const PathName& pconfName,
						 const PathName& pplugName)
			: module(pmodule), regPlugin(preg), pluginLoaderConfig(pconfig),
			  confName(getPool(), pconfName), plugName(getPool(), pplugName),
			  processingDelayedDelete(false), delay(DEFAULT_DELAY)
		{
			if (pluginLoaderConfig.hasData())
			{
				const ConfigFile::Parameter* p = pluginLoaderConfig->findParameter("ConfigFile");
				if (p && p->value.hasData())
				{
					confName = p->value.ToPathName();
				}
			}
			if (module != builtin)
			{
				byTypeCounters->get(module->getPlugin(regPlugin).type).counter++;
			}
#ifdef DEBUG_PLUGINS
			RegisteredPlugin& r(module->getPlugin(regPlugin));
			fprintf(stderr, " ConfiguredPlugin %s module %s registered as %s type %d order %d\n",
					plugName.c_str(), module->getName(), r.name.c_str(), r.type, regPlugin);
#endif
		}

		const char* getConfigFileName()
		{
			return confName.c_str();
		}

		IConfig* getDefaultConfig()
		{
			return findPluginConfig(pluginLoaderConfig, confName);
		}

		const PluginModule* getPluggedModule() const noexcept
		{
			return module;
		}

		IPluginBase* factory(IScratchBirdConf *iScratchBirdConf);

		const char* getPlugName()
		{
			return plugName.c_str();
		}

		void setReleaseDelay(ISC_UINT64 microSeconds) noexcept
		{
#ifdef DEBUG_PLUGINS
			fprintf(stderr, "Set delay for ConfiguredPlugin %s:%p\n", plugName.c_str(), this);
#endif
			delay = microSeconds > DEFAULT_DELAY ? microSeconds : DEFAULT_DELAY;
		}

		ISC_UINT64 getReleaseDelay()
		{
			return delay;
		}

		// ITimer implementation
		void handler()
		{ }

		int release();
		static void processDelayedDelete();

	private:
		~ConfiguredPlugin() {}
		void destroy();

		RefPtr<PluginModule> module;
		unsigned int regPlugin;
		RefPtr<ConfigFile> pluginLoaderConfig;
		PathName confName;
		PathName plugName;
		bool processingDelayedDelete;

		static const FB_UINT64 DEFAULT_DELAY = 1000000 * 60;		// 1 min
		FB_UINT64 delay;
	};

	// Provides per-database configuration from databases.conf.
	class FactoryParameter final :
		public RefCntIface<IPluginConfigImpl<FactoryParameter, CheckStatusWrapper> >
	{
	public:
		FactoryParameter(ConfiguredPlugin* cp, IScratchBirdConf* fc)
			: configuredPlugin(cp), firebirdConf(fc)
		{ }

		// IPluginConfig implementation
		const char* getConfigFileName()
		{
			return configuredPlugin->getConfigFileName();
		}

		IConfig* getDefaultConfig(CheckStatusWrapper* status)
		{
			try
			{
				return configuredPlugin->getDefaultConfig();
			}
			catch (const Exception& ex)
			{
				ex.stuffException(status);
				return NULL;
			}
		}

		IScratchBirdConf* getScratchBirdConf(CheckStatusWrapper* status)
		{
			try
			{
				if (!firebirdConf.hasData())
				{
					RefPtr<const Config> specificConf(Config::getDefaultConfig());
					firebirdConf = FB_NEW ScratchBirdConf(specificConf);
				}

				firebirdConf->addRef();
				return firebirdConf;
			}
			catch (const Exception& ex)
			{
				ex.stuffException(status);
				return NULL;
			}
		}

		void setReleaseDelay(CheckStatusWrapper*, ISC_UINT64 microSeconds)
		{
			configuredPlugin->setReleaseDelay(microSeconds);
		}

	private:
		~FactoryParameter()
		{
#ifdef DEBUG_PLUGINS
			fprintf(stderr, "~FactoryParameter places configuredPlugin %s in unload query for %lld seconds\n",
				configuredPlugin->getPlugName(), configuredPlugin->getReleaseDelay() / 1000000);
#endif
			FbLocalStatus ls;
			TimerInterfacePtr()->start(&ls, configuredPlugin, configuredPlugin->getReleaseDelay());
			// errors are ignored here - configuredPlugin will be released at once
		}

		RefPtr<ConfiguredPlugin> configuredPlugin;
		RefPtr<IScratchBirdConf> firebirdConf;
	};

	IPluginBase* ConfiguredPlugin::factory(IScratchBirdConf* firebirdConf)
	{
		FactoryParameter* par = FB_NEW FactoryParameter(this, firebirdConf);
		par->addRef();

		FbLocalStatus ls;
		IPluginBase* plugin = module->getPlugin(regPlugin).factory->createPlugin(&ls, par);

		if (plugin && !(ls->getState() & ScratchBird::IStatus::STATE_ERRORS))
		{
			plugin->setOwner(par);
			return plugin;
		}

		par->release();
		check(&ls);
		return NULL;
	}


	class MapKey : public AutoStorage
	{
		public:
			MapKey(unsigned int ptype, const PathName& pname)
				: type(ptype), name(getPool(), pname)
			{ }

			MapKey(MemoryPool& p, const MapKey& mk)
				: AutoStorage(p), type(mk.type), name(getPool(), mk.name)
			{ }

			bool operator<(const MapKey& c) const	{	return type < c.type || (type == c.type && name < c.name);	}
			bool operator==(const MapKey& c) const	{	return type == c.type && name == c.name;					}
			bool operator>(const MapKey& c) const	{	return type > c.type || (type == c.type && name > c.name);	}
		private:
			unsigned int type;
			PathName name;
	};

	static bool destroyingPluginsMap = false;

	class PluginsMap : public GenericMap<Pair<Left<MapKey, ConfiguredPlugin*> > >
	{
	public:
		explicit PluginsMap(MemoryPool& p)
			: GenericMap<Pair<Left<MapKey, ConfiguredPlugin*> > >(p), wakeIt(NULL)
		{
		}

		~PluginsMap()
		{
			MutexLockGuard g(mutex, FB_FUNCTION);

			destroyingPluginsMap = true;
			// unload plugins
			Accessor accessor(this);
			for (bool found = accessor.getFirst(); found; found = accessor.getNext())
			{
				ConfiguredPlugin* plugin = accessor.current()->second;
				plugin->release();
			}
		}

		Mutex mutex; // locked by this class' destructor and by objects that use the plugins var below.
		Semaphore* wakeIt;
	};

	GlobalPtr<PluginsMap> plugins;

	void ConfiguredPlugin::destroy()
	{
		// plugins->mutex must be entered by current thread

#ifdef DEV_BUILD
		if (!plugins->mutex.tryEnter(FB_FUNCTION))
			fb_assert(false);
		plugins->mutex.leave();
#endif
		if (!destroyingPluginsMap)
		{
			plugins->remove(MapKey(module->getPlugin(regPlugin).type, plugName));
		}

		fb_assert(!plugins->wakeIt);
		if (module != builtin)
		{
			unsigned int type = module->getPlugin(regPlugin).type;
			if (--(byTypeCounters->get(type).counter) == 0)
			{
				plugins->wakeIt = byTypeCounters->get(type).waitsOn;
			}
		}
#ifdef DEBUG_PLUGINS
		fprintf(stderr, "~ConfiguredPlugin %s type %d\n", plugName.c_str(), module->getPlugin(regPlugin).type);
#endif
	}

	typedef HalfStaticArray<ConfiguredPlugin*, 16> DelayedDelete;
	static GlobalPtr<DelayedDelete> delayedDelete;

	int ConfiguredPlugin::release()
	{
		int x = --refCounter;

#ifdef DEBUG_PLUGINS
		fprintf(stderr, "ConfiguredPlugin::release %s %d\n", plugName.c_str(), x);
#endif

		if (x == 0)
		{
			{ // plugins->mutex scope
				MutexLockGuard g(plugins->mutex, FB_FUNCTION);
				if (refCounter != 0)
					return 1;

				if (Why::timerThreadStopped() && !processingDelayedDelete && delayedDelete)
				{
					// delay delete
					addRef();
					delayedDelete->push(this);

					return 1;
				}

				destroy();
			}

			// Must run out of mutex scope to avoid deadlock with PluginManager::threadDetach()
			// called when module is unloaded by dtor
			delete this;
			return 0;
		}

		return 1;
	}

	void ConfiguredPlugin::processDelayedDelete()
	{
		DelayedDelete& dd(delayedDelete);
		MutexEnsureUnlock g(plugins->mutex, FB_FUNCTION);
		g.enter();
		for (unsigned n = 0; n < dd.getCount(); ++n)
		{
			ConfiguredPlugin* ptr = dd[n];
			if (ptr)
			{
				g.leave();
				ptr->processingDelayedDelete = true;
				ptr->release();
				g.enter();
			}
			dd[n] = nullptr;
		}
		delayedDelete->clear();
	}

	PluginModule* modules = NULL;

	PluginModule* current = NULL;

	PluginModule::PluginModule(ModuleLoader::Module* pmodule, const PathName& pname)
		: name(getPool(), pname), module(pmodule), cleanup(NULL), regPlugins(getPool()),
		  next(modules), prev(&modules)
	{
		if (next)
		{
			next->prev = &next;
		}
		*prev = this;
	}

	struct PluginLoadInfo
	{
		PathName curModule, regName, plugConfigFile;
		RefPtr<ConfigFile> conf;
		bool required;

		explicit PluginLoadInfo(const char* pluginName)
		{
			// define default values for plugin ...
			curModule = fb_utils::getPrefix(IConfigManager::DIR_PLUGINS, pluginName);
			regName = pluginName;
			required = false;

			// and try to load them from conf file
			conf = findInPluginsConf("Plugin", pluginName);

			if (conf.hasData())
			{
				const ConfigFile::Parameter* v = conf->findParameter("RegisterName");
				if (v)
				{
					regName = v->value.ToPathName();
				}

				v = conf->findParameter("Module");
				if (v)
				{
					curModule = v->value.ToPathName();
				}

				v = conf->findParameter("Required");
				if (v)
				{
					required = v->asBoolean();
				}
			}

			plugConfigFile = curModule;
			changeExtension(plugConfigFile, "conf");

			PathUtils::fixupSeparators(curModule);
			PathUtils::fixupSeparators(regName);
			PathUtils::fixupSeparators(plugConfigFile);
		}
	};


	// Provides access to plugins of given type / name.
	class PluginSet final : public RefCntIface<IPluginSetImpl<PluginSet, CheckStatusWrapper> >
	{
	public:
		// IPluginSet implementation
		const char* getName() const
		{
			return currentPlugin.hasData() ? currentName.c_str() : NULL;
		}

		const char* getModuleName() const
		{
			return currentPlugin.hasData() ? currentPlugin->getPluggedModule()->getName() : NULL;
		}

		void set(CheckStatusWrapper* status, const char* newName)
		{
			try
			{
				namesList = newName;
				namesList.alltrim(" \t");
				next(status);
			}
			catch (const ScratchBird::Exception& ex)
			{
				ex.stuffException(status);
			}
		}

		IPluginBase* getPlugin(CheckStatusWrapper* status);
		void next(CheckStatusWrapper* status);

		PluginSet(unsigned int pinterfaceType, const char* pnamesList,
				  IScratchBirdConf* fbConf)
			: interfaceType(pinterfaceType), namesList(getPool()),
			  currentName(getPool()), currentPlugin(NULL),
			  firebirdConf(fbConf)
		{
			namesList.assign(pnamesList);
			namesList.alltrim(" \t");
			FbLocalStatus statusWrapper;
			next(&statusWrapper);
			check(&statusWrapper);
		}

	private:
		unsigned int interfaceType;
		PathName namesList;

		PathName currentName;
		RefPtr<ConfiguredPlugin> currentPlugin;		// Missing data in this field indicates EOF

		RefPtr<IScratchBirdConf> firebirdConf;
		MasterInterfacePtr masterInterface;

		RefPtr<PluginModule> loadModule(const PluginLoadInfo& info);

		void loadError(const Arg::StatusVector& error)
		{
			(Arg::Gds(isc_pman_cannot_load_plugin) << currentName << error).raise();
		}

		static void CLOOP_CARG upFunction(void* /*interface*/, IStatus* status)
		{
			status->setErrors(Arg::Gds(isc_wish_list).value());
		}
	};

	// ************************************* //
	// ** next() - core of plugin manager ** //
	// ************************************* //
	void PluginSet::next(CheckStatusWrapper* status)
	{
		try
		{
			if (currentPlugin.hasData())
			{
				currentPlugin = NULL;
			}

			// Avoid concurrent load of the same module
			static Static<Mutex> loadModuleMutex;
			MutexLockGuard lmGuard(*(&loadModuleMutex), FB_FUNCTION);

			MutexLockGuard g(plugins->mutex, FB_FUNCTION);

			while (currentName.getWord(namesList, " \t,;"))
			{
				// First check - may be currentName is present among already configured plugins
				ConfiguredPlugin* tmp = NULL;
				if (plugins->get(MapKey(interfaceType, currentName), tmp))
				{
					currentPlugin = tmp;
					break;
				}

				// setup loadinfo
				PluginLoadInfo info(currentName.c_str());

				// Check if module is loaded and load it if needed
				RefPtr<PluginModule> m(modules->findModule(info.curModule));
				if (!m.hasData() && !flShutdown)
				{
					MutexUnlockGuard cout(plugins->mutex, FB_FUNCTION);
					m = loadModule(info);
				}
				if (!m.hasData())
				{
					continue;
				}

				int r = m->findPlugin(interfaceType, info.regName);
				if (r < 0)
				{
					loadError(Arg::Gds(isc_pman_plugin_notfound) <<
							  info.curModule << info.regName << Arg::Num(interfaceType));
				}

				currentPlugin = FB_NEW ConfiguredPlugin(m, r, info.conf, info.plugConfigFile, currentName);

				plugins->put(MapKey(interfaceType, currentName), currentPlugin);
				return;
			}
		}
		catch (const ScratchBird::Exception& ex)
		{
			ex.stuffException(status);
		}
	}

	RefPtr<PluginModule> PluginSet::loadModule(const PluginLoadInfo& info)
	{
		PathName fixedModuleName(info.curModule);
		ISC_STATUS_ARRAY statusArray;

		ModuleLoader::Module* module = nullptr;
		int step = 0;
		bool bad = false;

		do
		{
			module = ModuleLoader::loadModule(statusArray, fixedModuleName);
		} while (module == nullptr && // break on success
				 !(bad = ModuleLoader::isLoadableModule(fixedModuleName)) && // Break if module is found but is bad
				 ModuleLoader::doctorModuleExtension(fixedModuleName, step)); // Break if no further modifications of name is available

		if (!module)
		{
			if (bad)
			{
				loadError(Arg::Gds(isc_pman_module_bad) << fixedModuleName <<
					Arg::StatusVector(statusArray));
			}
			if (info.required)
			{
				loadError(Arg::Gds(isc_pman_module_notfound) << fixedModuleName <<
					Arg::StatusVector(statusArray));
			}

			return RefPtr<PluginModule>(NULL);
		}

		RefPtr<PluginModule> rc(FB_NEW PluginModule(module, info.curModule));
		typedef void PluginEntrypoint(IMaster* masterInterface);
		PluginEntrypoint* startModule;
		ISC_STATUS_ARRAY stArray;
		if (module->findSymbol(stArray, STRINGIZE(FB_PLUGIN_ENTRY_POINT), startModule))
		{
			current = rc;
			startModule(masterInterface);
			current = NULL;
#ifdef DARWIN	// Plugin unload disabled in MacOS - GH-7112
			rc->addRef();
#endif
			return rc;
		}

		loadError(Arg::Gds(isc_pman_entrypoint_notfound) << fixedModuleName << Arg::StatusVector(stArray));
		return RefPtr<PluginModule>(NULL);	// compiler warning silencer
	}

	IPluginBase* PluginSet::getPlugin(CheckStatusWrapper* status)
	{
		try
		{
			while (currentPlugin.hasData())
			{
				IPluginBase* p = currentPlugin->factory(firebirdConf);
				if (p)
					return p;

				next(status);
				if (status->getState() & ScratchBird::IStatus::STATE_ERRORS)
					break;
			}
		}
		catch (const ScratchBird::Exception& ex)
		{
			ex.stuffException(status);
		}

		return NULL;
	}

	class BuiltinRegister
	{
	public:
		static void init()
		{
			PluginManagerInterfacePtr pi;
			Remote::registerRedirector(pi);
		}

		static void cleanup()
		{
		}
	};
} // anonymous namespace


namespace ScratchBird {

PluginManager::PluginManager()
{
	MutexLockGuard g(plugins->mutex, FB_FUNCTION);

	if (!builtin)
	{
		builtin = FB_NEW PluginModule(NULL, "<builtin>");
		builtin->addRef();	// Will never be unloaded
		current = builtin;
	}
}


void PluginManager::registerPluginFactory(unsigned int interfaceType, const char* defaultName, IPluginFactory* factory)
{
	try
	{
		MutexLockGuard g(plugins->mutex, FB_FUNCTION);

		if (!current)
		{
			// not good time to call this function - ignore request
			gds__log("Unexpected call to register plugin %s, type %d - ignored\n", defaultName, interfaceType);
			return;
		}

		unsigned int r = current->addPlugin(RegisteredPlugin(factory, defaultName, interfaceType));

		if (current == builtin)
		{
			PathName plugConfigFile = fb_utils::getPrefix(IConfigManager::DIR_PLUGINS, defaultName);
			changeExtension(plugConfigFile, "conf");

			ConfiguredPlugin* p = FB_NEW ConfiguredPlugin(RefPtr<PluginModule>(builtin), r,
				findInPluginsConf("Plugin", defaultName), plugConfigFile, defaultName);
			p->addRef();  // Will never be unloaded
			plugins->put(MapKey(interfaceType, defaultName), p);
		}
	}
	catch(const Exception& ex)
	{
		// looks like something gone seriously wrong - therefore add more error handling here
		try
		{
			FbLocalStatus ls;
			ex.stuffException(&ls);
			char text[256];
			UtilInterfacePtr()->formatStatus(text, sizeof(text), &ls);
			Syslog::Record(Syslog::Error, text);

			iscLogException("Plugin registration error", ex);
		}
		catch(const BadAlloc&)
		{
			Syslog::Record(Syslog::Error, "Plugin registration error - out of memory");
		}
		catch(...)
		{
			Syslog::Record(Syslog::Error, "Double fault during plugin registration");
		}

#ifdef DEV_BUILD
		abort();
#endif
	}
}


void PluginManager::registerModule(IPluginModule* cleanup)
{
	MutexLockGuard g(plugins->mutex, FB_FUNCTION);

	if (!current)
	{
		// not good time to call this function - ignore request
		gds__log("Unexpected call to set module cleanup - ignored\n");
		return;
	}

	current->setCleanup(cleanup);
}

void PluginManager::unregisterModule(IPluginModule* cleanup)
{
	{	// guard scope
		MutexLockGuard g(plugins->mutex, FB_FUNCTION);
		modules->resetCleanup(cleanup);
	}

	// Module cleanup should be unregistered only if it's unloaded
	// and only if it's unloaded not by PluginManager, but by OS.
	// That means that task is closing unexpectedly - sooner of all
	// exit() is called by client of embedded server. Shutdown ourselves.

#ifdef WIN_NT
	if (!ScratchBird::dDllUnloadTID)
		ScratchBird::dDllUnloadTID = GetCurrentThreadId();
#endif

	fb_shutdown(10000, fb_shutrsn_exit_called);
}

IPluginSet* PluginManager::getPlugins(CheckStatusWrapper* status, unsigned int interfaceType,
	const char* namesList, IScratchBirdConf* firebirdConf)
{
	try
	{
		static InitMutex<BuiltinRegister> registerBuiltinPlugins("RegisterBuiltinPlugins");
		registerBuiltinPlugins.init();

		IPluginSet* rc = FB_NEW PluginSet(interfaceType, namesList, firebirdConf);
		rc->addRef();
		return rc;
	}
	catch (const Exception& ex)
	{
		ex.stuffException(status);
		return NULL;
	}
}


void PluginManager::releasePlugin(IPluginBase* plugin)
{
	IReferenceCounted* parent = plugin->getOwner();

	if (plugin->release() == 0)
	{
		///fb_assert(parent);
		if (parent)
		{
			parent->release();

			MutexLockGuard g(plugins->mutex, FB_FUNCTION);
			if (plugins->wakeIt)
			{
				plugins->wakeIt->release();
				plugins->wakeIt = NULL;
			}
		}
	}
}


IConfig* PluginManager::getConfig(CheckStatusWrapper* status, const char* filename)
{
	try
	{
		IConfig* rc = FB_NEW ConfigAccess(RefPtr<ConfigFile>(
			FB_NEW_POOL(*getDefaultMemoryPool()) ConfigFile(*getDefaultMemoryPool(),
				filename, ConfigFile::HAS_SUB_CONF)));
		rc->addRef();
		return rc;
	}
	catch (const ScratchBird::Exception& ex)
	{
		ex.stuffException(status);
	}
	return NULL;
}


void PluginManager::shutdown()
{
	flShutdown = true;
}

void PluginManager::waitForType(unsigned int typeThatMustGoAway)
{
	fb_assert(typeThatMustGoAway < PluginManager::TYPE_COUNT);

	Semaphore sem;
	Semaphore* semPtr = NULL;

	{ // guard scope
		MutexLockGuard g(plugins->mutex, FB_FUNCTION);

		if (byTypeCounters->get(typeThatMustGoAway).counter > 0)
		{
			fb_assert(!byTypeCounters->get(typeThatMustGoAway).waitsOn);
			byTypeCounters->get(typeThatMustGoAway).waitsOn = semPtr = &sem;
#ifdef DEBUG_PLUGINS
			fprintf(stderr, "PluginManager::waitForType %d\n", typeThatMustGoAway);
#endif
		}
#ifdef DEBUG_PLUGINS
		else
		{
			fprintf(stderr, "PluginManager: type %d is already gone\n", typeThatMustGoAway);
		}
#endif
	}

	if (semPtr)
	{
		semPtr->enter();
	}
}

void PluginManager::threadDetach()
{
	MutexLockGuard g(plugins->mutex, FB_FUNCTION);
	if (modules)
		modules->threadDetach();
}

void PluginManager::deleteDelayed()
{
	ConfiguredPlugin::processDelayedDelete();
}

}	// namespace ScratchBird

namespace {

class DataCache
{
public:
	explicit DataCache(MemoryPool &p)
		: cache(p), db(p)
	{
		cache.resize(IConfigManager::DIR_COUNT);
		for (unsigned i = 0; i < IConfigManager::DIR_COUNT; ++i)
		{
			cache[i] = fb_utils::getPrefix(i, "");
		}

		db = fb_utils::getPrefix(IConfigManager::DIR_SECDB, SECURITY_DB);
	}

	const char* getDir(unsigned code)
	{
		fb_assert(code < IConfigManager::DIR_COUNT);
		return cache[code].c_str();
	}

	const char* getDb()
	{
		return db.c_str();
	}

private:
	ObjectsArray<PathName> cache;
	PathName db;
};

InitInstance<DataCache> dataCache;

}	// anonymous namespace

namespace ScratchBird {

/******************************************************************************
*
*	Static instance of the root and install directories detector
*/

InitInstance<ConfigRoot> rootDetector;


// Generic access to all config interfaces
class ConfigManager : public AutoIface<IConfigManagerImpl<ConfigManager, CheckStatusWrapper> >
{
public:
	const char* getDirectory(unsigned code)
	{
		try
		{
			return dataCache().getDir(code);
		}
		catch (const Exception&)
		{
			return NULL;
		}
	}

	IConfig* getPluginConfig(const char* pluginName)
	{
		try
		{
			// setup loadinfo
			PluginLoadInfo info(pluginName);
			return findPluginConfig(info.conf, info.plugConfigFile);
		}
		catch (const Exception&)
		{
			return NULL;
		}
	}

	IScratchBirdConf* getScratchBirdConf()
	{
		try
		{
			return getScratchBirdConfig();
		}
		catch (const Exception&)
		{
			return NULL;
		}
	}

	IScratchBirdConf* getDatabaseConf(const char* dbName)
	{
		try
		{
			PathName dummy;
			ScratchBird::RefPtr<const ScratchBird::Config> config;
			expandDatabaseName(dbName, dummy, &config);

			IScratchBirdConf* firebirdConf = FB_NEW ScratchBirdConf(config);
			firebirdConf->addRef();
			return firebirdConf;
		}
		catch (const Exception&)
		{
			return NULL;
		}
	}

	const char* getInstallDirectory()
	{
		return rootDetector().getInstallDirectory();
	}

	const char* getRootDirectory()
	{
		return rootDetector().getRootDirectory();
	}

	const char* getDefaultSecurityDb()
	{
		return dataCache().getDb();
	}

};

static ConfigManager configManager;
IConfigManager* iConfigManager(&configManager);

}	// namespace ScratchBird
