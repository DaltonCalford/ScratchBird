# UDR (User Defined Routine) System Specification

## Overview

The UDR (User Defined Routine) system provides a plugin architecture for extending the database engine with custom functionality. Inspired by Firebird's UDR system, it allows loading external code as shared libraries that can implement functions, procedures, triggers, and external data sources.

## UDR Architecture

```
┌─────────────────────────────────────────────────────────┐
│  Database Engine                                         │
│                                                          │
│  ┌────────────────────────────────────────────────┐    │
│  │  UDR Manager                                    │    │
│  │  - Plugin discovery                             │    │
│  │  - Lifecycle management                         │    │
│  │  - Version compatibility                        │    │
│  └────────────┬───────────────────────────────────┘    │
│               │                                          │
│  ┌────────────▼───────────────────────────────────┐    │
│  │  UDR Registry                                   │    │
│  │  - Loaded plugins                               │    │
│  │  - Function/procedure catalog                   │    │
│  │  - Type mappings                                │    │
│  └────────────┬───────────────────────────────────┘    │
│               │                                          │
│  ┌────────────▼───────────────────────────────────┐    │
│  │  Query Executor                                 │    │
│  │  - Calls UDR functions                          │    │
│  │  - Manages UDR contexts                         │    │
│  └────────────────────────────────────────────────┘    │
└───────────────┬──────────────────────────────────────────┘
                │ dlopen/dlsym
                │
┌───────────────▼──────────────────────────────────────────┐
│  UDR Plugins (.so / .dll)                                │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  │
│  │ Math UDR     │  │ JSON UDR     │  │ Remote DB    │  │
│  │ - sin/cos    │  │ - parse      │  │ - connect    │  │
│  │ - sqrt       │  │ - extract    │  │ - query      │  │
│  └──────────────┘  └──────────────┘  └──────────────┘  │
└──────────────────────────────────────────────────────────┘
```

## Core UDR Interfaces

### Base UDR Interface

```c
// udr.h - Core UDR interface definitions

#ifndef UDR_H
#define UDR_H

#include <stdint.h>
#include <stdbool.h>

// UDR API version
#define UDR_API_VERSION_MAJOR 1
#define UDR_API_VERSION_MINOR 0
#define UDR_API_VERSION_PATCH 0

// UDR types
typedef enum {
    UDR_TYPE_FUNCTION,        // Scalar function
    UDR_TYPE_PROCEDURE,       // Stored procedure
    UDR_TYPE_AGGREGATE,       // Aggregate function
    UDR_TYPE_TRIGGER,         // Trigger function
    UDR_TYPE_EXTERNAL_TABLE,  // External data source
} UDRType;

// Status object for error handling
typedef struct IStatus {
    int error_code;
    char error_message[512];
    char error_detail[1024];
    const char* error_hint;
} IStatus;

// Status methods
static inline void status_init(IStatus* status) {
    status->error_code = 0;
    status->error_message[0] = '\0';
    status->error_detail[0] = '\0';
    status->error_hint = NULL;
}

static inline void status_set_error(IStatus* status, int code, 
                                    const char* message) {
    status->error_code = code;
    snprintf(status->error_message, sizeof(status->error_message), 
             "%s", message);
}

static inline bool status_is_success(IStatus* status) {
    return status->error_code == 0;
}

// Metadata interface
typedef struct IMetadataBuilder {
    void* context;
    
    // Set metadata
    void (*set_type)(void* context, int sql_type);
    void (*set_length)(void* context, int length);
    void (*set_precision)(void* context, int precision);
    void (*set_scale)(void* context, int scale);
    void (*set_charset)(void* context, const char* charset);
    void (*set_null)(void* context, bool nullable);
    void (*set_name)(void* context, const char* name);
} IMetadataBuilder;

// Message buffer for parameters and results
typedef struct IMessageMetadata {
    uint32_t field_count;
    
    // Field information
    const char* (*get_field_name)(struct IMessageMetadata* self, uint32_t index);
    int (*get_field_type)(struct IMessageMetadata* self, uint32_t index);
    int (*get_field_length)(struct IMessageMetadata* self, uint32_t index);
    bool (*is_nullable)(struct IMessageMetadata* self, uint32_t index);
} IMessageMetadata;

typedef struct IMessageBuffer {
    void* data;
    IMessageMetadata* metadata;
    
    // Value accessors
    bool (*is_null)(struct IMessageBuffer* self, uint32_t index);
    void (*set_null)(struct IMessageBuffer* self, uint32_t index, bool null);
    
    // Get values
    int16_t (*get_short)(struct IMessageBuffer* self, uint32_t index);
    int32_t (*get_int)(struct IMessageBuffer* self, uint32_t index);
    int64_t (*get_long)(struct IMessageBuffer* self, uint32_t index);
    float (*get_float)(struct IMessageBuffer* self, uint32_t index);
    double (*get_double)(struct IMessageBuffer* self, uint32_t index);
    const char* (*get_string)(struct IMessageBuffer* self, uint32_t index);
    const void* (*get_bytes)(struct IMessageBuffer* self, uint32_t index, uint32_t* length);
    
    // Set values
    void (*set_short)(struct IMessageBuffer* self, uint32_t index, int16_t value);
    void (*set_int)(struct IMessageBuffer* self, uint32_t index, int32_t value);
    void (*set_long)(struct IMessageBuffer* self, uint32_t index, int64_t value);
    void (*set_float)(struct IMessageBuffer* self, uint32_t index, float value);
    void (*set_double)(struct IMessageBuffer* self, uint32_t index, double value);
    void (*set_string)(struct IMessageBuffer* self, uint32_t index, const char* value);
    void (*set_bytes)(struct IMessageBuffer* self, uint32_t index, const void* value, uint32_t length);
} IMessageBuffer;

// Context interface (provides engine services to UDR)
typedef struct IContext {
    void* engine_context;
    
    // Memory allocation
    void* (*allocate)(struct IContext* self, size_t size);
    void (*deallocate)(struct IContext* self, void* ptr);
    
    // Logging
    void (*log_info)(struct IContext* self, const char* message);
    void (*log_warning)(struct IContext* self, const char* message);
    void (*log_error)(struct IContext* self, const char* message);
    
    // Transaction info
    uint64_t (*get_transaction_id)(struct IContext* self);
    
    // Query execution (for UDRs that need to query the database)
    void* (*execute_sql)(struct IContext* self, const char* sql, IStatus* status);
} IContext;

// External function interface
typedef struct IExternalFunction {
    // Setup (called once per statement preparation)
    void (*setup)(struct IExternalFunction* self,
                  IStatus* status,
                  IContext* context,
                  IMetadataBuilder* in_builder,
                  IMetadataBuilder* out_builder);
    
    // Execute (called for each invocation)
    void (*execute)(struct IExternalFunction* self,
                    IStatus* status,
                    IContext* context,
                    IMessageBuffer* in_msg,
                    IMessageBuffer* out_msg);
    
    // Cleanup
    void (*dispose)(struct IExternalFunction* self);
} IExternalFunction;

// External procedure interface
typedef struct IExternalProcedure {
    // Setup
    void (*setup)(struct IExternalProcedure* self,
                  IStatus* status,
                  IContext* context,
                  IMetadataBuilder* in_builder,
                  IMetadataBuilder* out_builder);
    
    // Open result set
    void (*open)(struct IExternalProcedure* self,
                 IStatus* status,
                 IContext* context,
                 IMessageBuffer* in_msg,
                 IMessageBuffer* out_msg);
    
    // Fetch next row
    bool (*fetch)(struct IExternalProcedure* self,
                  IStatus* status,
                  IMessageBuffer* out_msg);
    
    // Close result set
    void (*close)(struct IExternalProcedure* self,
                  IStatus* status);
    
    // Cleanup
    void (*dispose)(struct IExternalProcedure* self);
} IExternalProcedure;

// External trigger interface
typedef struct IExternalTrigger {
    // Setup
    void (*setup)(struct IExternalTrigger* self,
                  IStatus* status,
                  IContext* context,
                  IMetadataBuilder* fields_builder);
    
    // Execute trigger
    void (*execute)(struct IExternalTrigger* self,
                    IStatus* status,
                    IContext* context,
                    int action,  // INSERT, UPDATE, DELETE
                    IMessageBuffer* old_values,
                    IMessageBuffer* new_values);
    
    // Cleanup
    void (*dispose)(struct IExternalTrigger* self);
} IExternalTrigger;

// Plugin factory interface
typedef struct IPluginFactory {
    // Create function instance
    IExternalFunction* (*create_function)(
        struct IPluginFactory* self,
        IStatus* status,
        IContext* context,
        const char* function_name);
    
    // Create procedure instance
    IExternalProcedure* (*create_procedure)(
        struct IPluginFactory* self,
        IStatus* status,
        IContext* context,
        const char* procedure_name);
    
    // Create trigger instance
    IExternalTrigger* (*create_trigger)(
        struct IPluginFactory* self,
        IStatus* status,
        IContext* context,
        const char* trigger_name);
} IPluginFactory;

// Plugin module interface (entry point)
typedef struct IPluginModule {
    uint32_t version_major;
    uint32_t version_minor;
    uint32_t version_patch;
    
    const char* (*get_name)(struct IPluginModule* self);
    const char* (*get_description)(struct IPluginModule* self);
    const char* (*get_author)(struct IPluginModule* self);
    
    // Initialize plugin
    void (*initialize)(struct IPluginModule* self,
                       IStatus* status,
                       IContext* context);
    
    // Get factory
    IPluginFactory* (*get_factory)(struct IPluginModule* self);
    
    // Shutdown plugin
    void (*shutdown)(struct IPluginModule* self);
} IPluginModule;

// Plugin entry point (must be exported by plugin .so/.dll)
#define UDR_PLUGIN_ENTRY fb_udr_plugin

// Plugin must export this function
IPluginModule* fb_udr_plugin(void);

#endif // UDR_H
```

## UDR Manager Implementation

```c
// udr_manager.h

#ifndef UDR_MANAGER_H
#define UDR_MANAGER_H

#include "udr.h"
#include <dlfcn.h>

// Loaded plugin information
typedef struct LoadedPlugin {
    char name[128];
    char path[512];
    void* handle;                    // dlopen handle
    IPluginModule* module;
    IPluginFactory* factory;
    
    // Reference counting for lifecycle
    uint32_t refcount;
    
    // Registration time
    uint64_t loaded_at;
} LoadedPlugin;

// UDR Manager
typedef struct UDRManager {
    // Plugin directory
    char plugin_dir[512];
    
    // Loaded plugins
    HashMap<const char*, LoadedPlugin*> plugins;
    pthread_rwlock_t plugins_lock;
    
    // Registered functions/procedures
    HashMap<const char*, UDRRegistration*> registrations;
    pthread_rwlock_t registrations_lock;
} UDRManager;

// UDR registration
typedef struct UDRRegistration {
    char name[128];
    UDRType type;
    char plugin_name[128];
    char entry_point[128];
    
    // Metadata
    IMessageMetadata* input_metadata;
    IMessageMetadata* output_metadata;
} UDRRegistration;

// Initialize UDR manager
UDRManager* udr_manager_create(const char* plugin_dir);

// Load plugin from shared library
int udr_manager_load_plugin(UDRManager* mgr, 
                            const char* plugin_path,
                            IStatus* status);

// Unload plugin
int udr_manager_unload_plugin(UDRManager* mgr, 
                              const char* plugin_name,
                              IStatus* status);

// Register function/procedure
int udr_manager_register(UDRManager* mgr,
                        UDRRegistration* registration,
                        IStatus* status);

// Unregister
int udr_manager_unregister(UDRManager* mgr,
                          const char* name,
                          IStatus* status);

// Get factory for plugin
IPluginFactory* udr_manager_get_factory(UDRManager* mgr,
                                        const char* plugin_name);

// List loaded plugins
List<const char*>* udr_manager_list_plugins(UDRManager* mgr);

// Destroy manager
void udr_manager_destroy(UDRManager* mgr);

#endif // UDR_MANAGER_H
```

```c
// udr_manager.c

#include "udr_manager.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

UDRManager* udr_manager_create(const char* plugin_dir) {
    UDRManager* mgr = malloc(sizeof(UDRManager));
    
    strncpy(mgr->plugin_dir, plugin_dir, sizeof(mgr->plugin_dir));
    
    // Initialize hash maps
    hashmap_init(&mgr->plugins);
    hashmap_init(&mgr->registrations);
    
    // Initialize locks
    pthread_rwlock_init(&mgr->plugins_lock, NULL);
    pthread_rwlock_init(&mgr->registrations_lock, NULL);
    
    return mgr;
}

int udr_manager_load_plugin(UDRManager* mgr, 
                            const char* plugin_path,
                            IStatus* status) {
    status_init(status);
    
    // 1. Load shared library
    void* handle = dlopen(plugin_path, RTLD_LAZY | RTLD_LOCAL);
    if (!handle) {
        status_set_error(status, -1, dlerror());
        return -1;
    }
    
    // 2. Get plugin entry point
    typedef IPluginModule* (*plugin_entry_fn)(void);
    plugin_entry_fn entry = (plugin_entry_fn)dlsym(handle, "fb_udr_plugin");
    
    if (!entry) {
        status_set_error(status, -2, 
                        "Plugin missing fb_udr_plugin entry point");
        dlclose(handle);
        return -1;
    }
    
    // 3. Get plugin module
    IPluginModule* module = entry();
    if (!module) {
        status_set_error(status, -3, "Plugin entry point returned NULL");
        dlclose(handle);
        return -1;
    }
    
    // 4. Verify version compatibility
    if (module->version_major != UDR_API_VERSION_MAJOR) {
        status_set_error(status, -4, 
                        "Plugin API version incompatible");
        dlclose(handle);
        return -1;
    }
    
    // 5. Initialize plugin
    IContext* init_context = create_init_context(mgr);
    module->initialize(module, status, init_context);
    
    if (!status_is_success(status)) {
        destroy_context(init_context);
        dlclose(handle);
        return -1;
    }
    
    destroy_context(init_context);
    
    // 6. Get factory
    IPluginFactory* factory = module->get_factory(module);
    if (!factory) {
        status_set_error(status, -5, "Plugin has no factory");
        module->shutdown(module);
        dlclose(handle);
        return -1;
    }
    
    // 7. Register plugin
    LoadedPlugin* plugin = malloc(sizeof(LoadedPlugin));
    strncpy(plugin->name, module->get_name(module), sizeof(plugin->name));
    strncpy(plugin->path, plugin_path, sizeof(plugin->path));
    plugin->handle = handle;
    plugin->module = module;
    plugin->factory = factory;
    plugin->refcount = 1;
    plugin->loaded_at = time(NULL);
    
    pthread_rwlock_wrlock(&mgr->plugins_lock);
    hashmap_put(&mgr->plugins, plugin->name, plugin);
    pthread_rwlock_unlock(&mgr->plugins_lock);
    
    log_info("Loaded UDR plugin: %s from %s", plugin->name, plugin_path);
    
    return 0;
}

int udr_manager_unload_plugin(UDRManager* mgr, 
                              const char* plugin_name,
                              IStatus* status) {
    status_init(status);
    
    pthread_rwlock_wrlock(&mgr->plugins_lock);
    
    LoadedPlugin* plugin = hashmap_get(&mgr->plugins, plugin_name);
    if (!plugin) {
        pthread_rwlock_unlock(&mgr->plugins_lock);
        status_set_error(status, -1, "Plugin not found");
        return -1;
    }
    
    // Check if plugin is still in use
    if (plugin->refcount > 1) {
        pthread_rwlock_unlock(&mgr->plugins_lock);
        status_set_error(status, -2, 
                        "Plugin still in use (active references)");
        return -1;
    }
    
    // Shutdown plugin
    plugin->module->shutdown(plugin->module);
    
    // Unload library
    dlclose(plugin->handle);
    
    // Remove from registry
    hashmap_remove(&mgr->plugins, plugin_name);
    
    pthread_rwlock_unlock(&mgr->plugins_lock);
    
    free(plugin);
    
    log_info("Unloaded UDR plugin: %s", plugin_name);
    
    return 0;
}

int udr_manager_register(UDRManager* mgr,
                        UDRRegistration* registration,
                        IStatus* status) {
    status_init(status);
    
    // Verify plugin exists
    pthread_rwlock_rdlock(&mgr->plugins_lock);
    LoadedPlugin* plugin = hashmap_get(&mgr->plugins, registration->plugin_name);
    pthread_rwlock_unlock(&mgr->plugins_lock);
    
    if (!plugin) {
        status_set_error(status, -1, "Plugin not loaded");
        return -1;
    }
    
    // Register
    pthread_rwlock_wrlock(&mgr->registrations_lock);
    
    // Check for duplicate
    if (hashmap_contains(&mgr->registrations, registration->name)) {
        pthread_rwlock_unlock(&mgr->registrations_lock);
        status_set_error(status, -2, "UDR already registered with this name");
        return -1;
    }
    
    hashmap_put(&mgr->registrations, registration->name, registration);
    
    pthread_rwlock_unlock(&mgr->registrations_lock);
    
    log_info("Registered UDR: %s (plugin: %s, type: %d)",
             registration->name,
             registration->plugin_name,
             registration->type);
    
    return 0;
}

IPluginFactory* udr_manager_get_factory(UDRManager* mgr,
                                        const char* plugin_name) {
    pthread_rwlock_rdlock(&mgr->plugins_lock);
    LoadedPlugin* plugin = hashmap_get(&mgr->plugins, plugin_name);
    pthread_rwlock_unlock(&mgr->plugins_lock);
    
    if (!plugin) {
        return NULL;
    }
    
    return plugin->factory;
}
```

## UDR Execution Context

```c
// udr_executor.h

#ifndef UDR_EXECUTOR_H
#define UDR_EXECUTOR_H

#include "udr.h"
#include "udr_manager.h"

// UDR execution context
typedef struct UDRExecutionContext {
    UDRManager* manager;
    IContext* engine_context;
    
    // Current execution
    LoadedPlugin* current_plugin;
    UDRRegistration* current_registration;
    
    // Instance
    union {
        IExternalFunction* function;
        IExternalProcedure* procedure;
        IExternalTrigger* trigger;
    } instance;
    
    // Message buffers
    IMessageBuffer* input_buffer;
    IMessageBuffer* output_buffer;
} UDRExecutionContext;

// Create execution context
UDRExecutionContext* udr_create_exec_context(
    UDRManager* manager,
    const char* udr_name,
    IStatus* status);

// Execute function
int udr_execute_function(
    UDRExecutionContext* ctx,
    IMessageBuffer* input,
    IMessageBuffer* output,
    IStatus* status);

// Execute procedure (returns result set)
ResultSet* udr_execute_procedure(
    UDRExecutionContext* ctx,
    IMessageBuffer* input,
    IStatus* status);

// Destroy execution context
void udr_destroy_exec_context(UDRExecutionContext* ctx);

#endif // UDR_EXECUTOR_H
```

```c
// udr_executor.c

#include "udr_executor.h"

UDRExecutionContext* udr_create_exec_context(
    UDRManager* manager,
    const char* udr_name,
    IStatus* status) {
    
    status_init(status);
    
    // 1. Look up registration
    pthread_rwlock_rdlock(&manager->registrations_lock);
    UDRRegistration* registration = hashmap_get(&manager->registrations, udr_name);
    pthread_rwlock_unlock(&manager->registrations_lock);
    
    if (!registration) {
        status_set_error(status, -1, "UDR not found");
        return NULL;
    }
    
    // 2. Get plugin factory
    IPluginFactory* factory = udr_manager_get_factory(
        manager,
        registration->plugin_name
    );
    
    if (!factory) {
        status_set_error(status, -2, "Plugin factory not available");
        return NULL;
    }
    
    // 3. Create context
    UDRExecutionContext* ctx = malloc(sizeof(UDRExecutionContext));
    ctx->manager = manager;
    ctx->engine_context = create_engine_context();
    ctx->current_registration = registration;
    
    // 4. Create instance based on type
    switch (registration->type) {
        case UDR_TYPE_FUNCTION:
            ctx->instance.function = factory->create_function(
                factory,
                status,
                ctx->engine_context,
                registration->entry_point
            );
            
            if (!ctx->instance.function) {
                free(ctx);
                return NULL;
            }
            
            // Setup function
            IMetadataBuilder* in_builder = create_metadata_builder();
            IMetadataBuilder* out_builder = create_metadata_builder();
            
            ctx->instance.function->setup(
                ctx->instance.function,
                status,
                ctx->engine_context,
                in_builder,
                out_builder
            );
            
            destroy_metadata_builder(in_builder);
            destroy_metadata_builder(out_builder);
            
            break;
            
        case UDR_TYPE_PROCEDURE:
            ctx->instance.procedure = factory->create_procedure(
                factory,
                status,
                ctx->engine_context,
                registration->entry_point
            );
            
            if (!ctx->instance.procedure) {
                free(ctx);
                return NULL;
            }
            break;
            
        case UDR_TYPE_TRIGGER:
            ctx->instance.trigger = factory->create_trigger(
                factory,
                status,
                ctx->engine_context,
                registration->entry_point
            );
            
            if (!ctx->instance.trigger) {
                free(ctx);
                return NULL;
            }
            break;
            
        default:
            status_set_error(status, -3, "Unsupported UDR type");
            free(ctx);
            return NULL;
    }
    
    // 5. Create message buffers
    ctx->input_buffer = create_message_buffer(registration->input_metadata);
    ctx->output_buffer = create_message_buffer(registration->output_metadata);
    
    return ctx;
}

int udr_execute_function(
    UDRExecutionContext* ctx,
    IMessageBuffer* input,
    IMessageBuffer* output,
    IStatus* status) {
    
    status_init(status);
    
    if (ctx->current_registration->type != UDR_TYPE_FUNCTION) {
        status_set_error(status, -1, "Not a function");
        return -1;
    }
    
    // Execute
    ctx->instance.function->execute(
        ctx->instance.function,
        status,
        ctx->engine_context,
        input,
        output
    );
    
    return status_is_success(status) ? 0 : -1;
}

ResultSet* udr_execute_procedure(
    UDRExecutionContext* ctx,
    IMessageBuffer* input,
    IStatus* status) {
    
    status_init(status);
    
    if (ctx->current_registration->type != UDR_TYPE_PROCEDURE) {
        status_set_error(status, -1, "Not a procedure");
        return NULL;
    }
    
    // Open procedure
    ctx->instance.procedure->open(
        ctx->instance.procedure,
        status,
        ctx->engine_context,
        input,
        ctx->output_buffer
    );
    
    if (!status_is_success(status)) {
        return NULL;
    }
    
    // Fetch all rows
    ResultSet* result = create_result_set();
    
    while (true) {
        bool has_more = ctx->instance.procedure->fetch(
            ctx->instance.procedure,
            status,
            ctx->output_buffer
        );
        
        if (!status_is_success(status)) {
            destroy_result_set(result);
            return NULL;
        }
        
        if (!has_more) {
            break;
        }
        
        // Copy row to result set
        result_set_add_row(result, ctx->output_buffer);
    }
    
    // Close procedure
    ctx->instance.procedure->close(
        ctx->instance.procedure,
        status
    );
    
    return result;
}

void udr_destroy_exec_context(UDRExecutionContext* ctx) {
    if (!ctx) return;
    
    // Dispose instance
    switch (ctx->current_registration->type) {
        case UDR_TYPE_FUNCTION:
            if (ctx->instance.function) {
                ctx->instance.function->dispose(ctx->instance.function);
            }
            break;
        case UDR_TYPE_PROCEDURE:
            if (ctx->instance.procedure) {
                ctx->instance.procedure->dispose(ctx->instance.procedure);
            }
            break;
        case UDR_TYPE_TRIGGER:
            if (ctx->instance.trigger) {
                ctx->instance.trigger->dispose(ctx->instance.trigger);
            }
            break;
    }
    
    // Destroy buffers
    destroy_message_buffer(ctx->input_buffer);
    destroy_message_buffer(ctx->output_buffer);
    
    // Destroy context
    destroy_engine_context(ctx->engine_context);
    
    free(ctx);
}
```

## SQL Registration Syntax

```sql
-- Load a plugin
LOAD PLUGIN 'remote_database' 
FROM '/opt/db/plugins/remote_database.so';

-- Register a function
CREATE FUNCTION calculate_distance(
    lat1 DOUBLE,
    lon1 DOUBLE,
    lat2 DOUBLE,
    lon2 DOUBLE
) RETURNS DOUBLE
EXTERNAL NAME 'geo_functions!haversine_distance'
ENGINE UDR;

-- Register a procedure (returns result set)
CREATE PROCEDURE list_remote_tables(
    database_name VARCHAR(128)
) RETURNS (
    schema_name VARCHAR(128),
    table_name VARCHAR(128),
    row_count BIGINT
)
EXTERNAL NAME 'remote_database!list_tables'
ENGINE UDR;

-- Register a trigger
CREATE TRIGGER audit_user_changes
AFTER UPDATE ON users
FOR EACH ROW
EXTERNAL NAME 'audit_plugin!log_change'
ENGINE UDR;

-- Unregister
DROP FUNCTION calculate_distance;
DROP PROCEDURE list_remote_tables;
DROP TRIGGER audit_user_changes;

-- Unload plugin
UNLOAD PLUGIN 'remote_database';
```

## Example: Simple Math UDR Plugin

```c
// example_math_plugin.c

#include "udr.h"
#include <math.h>
#include <stdlib.h>

// Function implementation
typedef struct MathFunction {
    IExternalFunction base;
    const char* function_name;
} MathFunction;

static void math_function_setup(IExternalFunction* self,
                                IStatus* status,
                                IContext* context,
                                IMetadataBuilder* in_builder,
                                IMetadataBuilder* out_builder) {
    // No setup needed for math functions
}

static void math_function_execute(IExternalFunction* self,
                                  IStatus* status,
                                  IContext* context,
                                  IMessageBuffer* in_msg,
                                  IMessageBuffer* out_msg) {
    MathFunction* func = (MathFunction*)self;
    
    status_init(status);
    
    // Check for NULL input
    if (in_msg->is_null(in_msg, 0)) {
        out_msg->set_null(out_msg, 0, true);
        return;
    }
    
    double input = in_msg->get_double(in_msg, 0);
    double result = 0.0;
    
    // Execute appropriate math function
    if (strcmp(func->function_name, "sqrt") == 0) {
        result = sqrt(input);
    } else if (strcmp(func->function_name, "sin") == 0) {
        result = sin(input);
    } else if (strcmp(func->function_name, "cos") == 0) {
        result = cos(input);
    } else if (strcmp(func->function_name, "exp") == 0) {
        result = exp(input);
    } else if (strcmp(func->function_name, "log") == 0) {
        if (input <= 0) {
            status_set_error(status, -1, "log() requires positive input");
            return;
        }
        result = log(input);
    } else {
        status_set_error(status, -2, "Unknown function");
        return;
    }
    
    out_msg->set_double(out_msg, 0, result);
}

static void math_function_dispose(IExternalFunction* self) {
    free(self);
}

// Factory
typedef struct MathFactory {
    IPluginFactory base;
} MathFactory;

static IExternalFunction* math_factory_create_function(
    IPluginFactory* self,
    IStatus* status,
    IContext* context,
    const char* function_name) {
    
    MathFunction* func = malloc(sizeof(MathFunction));
    func->base.setup = math_function_setup;
    func->base.execute = math_function_execute;
    func->base.dispose = math_function_dispose;
    func->function_name = function_name;
    
    return (IExternalFunction*)func;
}

static MathFactory factory = {
    .base = {
        .create_function = math_factory_create_function,
        .create_procedure = NULL,
        .create_trigger = NULL
    }
};

// Module
typedef struct MathModule {
    IPluginModule base;
} MathModule;

static const char* math_module_get_name(IPluginModule* self) {
    return "math_functions";
}

static const char* math_module_get_description(IPluginModule* self) {
    return "Advanced mathematical functions";
}

static const char* math_module_get_author(IPluginModule* self) {
    return "Database Team";
}

static void math_module_initialize(IPluginModule* self,
                                   IStatus* status,
                                   IContext* context) {
    status_init(status);
    context->log_info(context, "Math plugin initialized");
}

static IPluginFactory* math_module_get_factory(IPluginModule* self) {
    return (IPluginFactory*)&factory;
}

static void math_module_shutdown(IPluginModule* self) {
    // Cleanup if needed
}

static MathModule module = {
    .base = {
        .version_major = UDR_API_VERSION_MAJOR,
        .version_minor = UDR_API_VERSION_MINOR,
        .version_patch = UDR_API_VERSION_PATCH,
        .get_name = math_module_get_name,
        .get_description = math_module_get_description,
        .get_author = math_module_get_author,
        .initialize = math_module_initialize,
        .get_factory = math_module_get_factory,
        .shutdown = math_module_shutdown
    }
};

// Plugin entry point
IPluginModule* fb_udr_plugin(void) {
    return (IPluginModule*)&module;
}
```

## Security Considerations

### Sandboxing

```c
// udr_sandbox.h - Security sandboxing for UDR plugins

typedef struct UDRSandbox {
    // Resource limits
    uint64_t max_memory_bytes;
    uint32_t max_execution_time_ms;
    uint32_t max_file_handles;
    
    // Allowed operations
    bool allow_network_access;
    bool allow_file_system_access;
    bool allow_subprocess;
    
    // Monitored resources
    uint64_t current_memory_usage;
    uint32_t current_file_handles;
} UDRSandbox;

// Apply sandbox restrictions before UDR execution
int udr_apply_sandbox(UDRSandbox* sandbox);

// Check resource limits during execution
int udr_check_limits(UDRSandbox* sandbox);
```

### Validation

```c
// Validate plugin before loading
int validate_udr_plugin(const char* plugin_path) {
    // 1. Check file permissions (should not be world-writable)
    struct stat st;
    if (stat(plugin_path, &st) != 0) {
        return -1;
    }
    
    if (st.st_mode & S_IWOTH) {
        log_error("Plugin file is world-writable: %s", plugin_path);
        return -1;
    }
    
    // 2. Verify file signature/checksum (if using signed plugins)
    if (!verify_plugin_signature(plugin_path)) {
        log_error("Plugin signature verification failed: %s", plugin_path);
        return -1;
    }
    
    // 3. Scan for obvious issues
    // (could integrate with static analysis tools)
    
    return 0;
}
```

## Configuration

```yaml
# /etc/db/udr.yaml

udr:
  # Plugin directory
  plugin_dir: /opt/db/plugins
  
  # Auto-load plugins on startup
  auto_load:
    - remote_database
    - math_functions
    - json_functions
  
  # Security
  security:
    enable_sandboxing: true
    max_memory_per_udr: 512M
    max_execution_time_ms: 30000
    allow_network_access: false
    allow_file_access: false
    require_signed_plugins: false
  
  # Logging
  logging:
    log_udr_calls: true
    log_level: info
```

## Best Practices

### For Plugin Developers

1. **Always initialize status**: Call `status_init()` at start of every function
2. **Check for NULL**: Always check if input values are NULL
3. **Memory management**: Use `IContext->allocate()` for memory that needs to survive the call
4. **Error handling**: Set appropriate error codes and messages in status
5. **Thread safety**: Don't use global variables; use instance variables
6. **Resource cleanup**: Always implement `dispose()` properly

### For Engine Integration

1. **Version checking**: Always verify API version compatibility
2. **Reference counting**: Track plugin usage before unloading
3. **Transaction awareness**: Pass transaction context to UDRs
4. **Timeout handling**: Implement execution timeouts
5. **Resource limits**: Monitor memory and CPU usage
6. **Error propagation**: Properly propagate UDR errors to client

This UDR system provides a robust, Firebird-inspired plugin architecture that will be the foundation for the Remote Database UDR in the next specification document.
