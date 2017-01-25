#include <GarrysMod/Lua/Interface.h>
#include <GarrysMod/Interfaces.hpp>
#include <lua.hpp>
#include <cstdint>
#include <hackedconvar.h>

#if defined CONCOMMANDX_SERVER

#include <eiface.h>

#elif defined CONCOMMANDX_CLIENT

#include <cdll_int.h>

#endif

namespace global
{

static SourceSDK::FactoryLoader icvar_loader( "vstdlib", true, IS_SERVERSIDE, "bin/" );
static SourceSDK::FactoryLoader engine_loader( "engine", false, IS_SERVERSIDE, "bin/" );

#if defined CONCOMMANDX_SERVER

static const char *ivengine_name = "VEngineServer021";

typedef IVEngineServer IVEngine;

#elif defined CONCOMMANDX_CLIENT

static const char *ivengine_name = "VEngineClient015";

typedef IVEngineClient IVEngine;

#endif

static ICvar *icvar = nullptr;
static IVEngine *ivengine = nullptr;

static void Initialize( lua_State *state )
{
	icvar = icvar_loader.GetInterface<ICvar>( CVAR_INTERFACE_VERSION );
	if( icvar == nullptr )
		LUA->ThrowError( "ICVar not initialized. Critical error." );

	ivengine = engine_loader.GetInterface<IVEngine>( ivengine_name );
	if( ivengine == nullptr )
		LUA->ThrowError( "IVEngineServer/Client not initialized. Critical error." );
}

}

namespace concommand
{

struct UserData
{
	ConCommand *cmd;
	uint8_t type;
	const char *name_original;
	char name[64];
	const char *help_original;
	char help[256];
};

static const char *metaname = "concommand";
static uint8_t metatype = GarrysMod::Lua::Type::COUNT + 2;
static const char *invalid_error = "invalid concommand";
static const char *table_name = "concommands_objects";

inline void CheckType( lua_State *state, int32_t index )
{
	if( !LUA->IsType( index, metatype ) )
		luaL_typerror( state, index, metaname );
}

inline UserData *GetUserdata( lua_State *state, int32_t index )
{
	return static_cast<UserData *>( LUA->GetUserdata( index ) );
}

static ConCommand *Get( lua_State *state, int32_t index )
{
	CheckType( state, index );
	ConCommand *command = static_cast<UserData *>( LUA->GetUserdata( index ) )->cmd;
	if( command == nullptr )
		LUA->ArgError( index, invalid_error );

	return command;
}

inline void Push( lua_State *state, ConCommand *command )
{
	if( command == nullptr )
	{
		LUA->PushNil( );
		return;
	}

	LUA->GetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );
	LUA->PushUserdata( command );
	LUA->GetTable( -2 );
	if( LUA->IsType( -1, metatype ) )
	{
		LUA->Remove( -2 );
		return;
	}

	LUA->Pop( 1 );

	UserData *udata = static_cast<UserData *>( LUA->NewUserdata( sizeof( UserData ) ) );
	udata->cmd = command;
	udata->type = metatype;
	udata->name_original = command->m_pszName;
	udata->help_original = command->m_pszHelpString;

	LUA->CreateMetaTableType( metaname, metatype );
	LUA->SetMetaTable( -2 );

	LUA->CreateTable( );
	lua_setfenv( state, -2 );

	LUA->PushUserdata( command );
	LUA->Push( -2 );
	LUA->SetTable( -4 );
	LUA->Remove( -2 );
}

inline ConCommand *Destroy( lua_State *state, int32_t index )
{
	UserData *udata = GetUserdata( state, 1 );
	ConCommand *command = udata->cmd;
	if( command == nullptr )
		return nullptr;

	LUA->GetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );
	LUA->PushUserdata( command );
	LUA->PushNil( );
	LUA->SetTable( -2 );
	LUA->Pop( 1 );

	command->m_pszName = udata->name_original;
	command->m_pszHelpString = udata->help_original;
	udata->cmd = nullptr;

	return command;
}

LUA_FUNCTION_STATIC( gc )
{
	if( !LUA->IsType( 1, metatype ) )
		return 0;

	Destroy( state, 1 );
	return 0;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->PushBool( Get( state, 1 ) == Get( state, 2 ) );
	return 1;
}

LUA_FUNCTION_STATIC( tostring )
{
	lua_pushfstring( state, "%s: %p", metaname, Get( state, 1 ) );
	return 1;
}

LUA_FUNCTION_STATIC( index )
{
	LUA->GetMetaTable( 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	if( !LUA->IsType( -1, GarrysMod::Lua::Type::NIL ) )
		return 1;

	LUA->Pop( 2 );

	lua_getfenv( state, 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	return 1;
}

LUA_FUNCTION_STATIC( newindex )
{
	lua_getfenv( state, 1 );
	LUA->Push( 2 );
	LUA->Push( 3 );
	LUA->RawSet( -3 );
	return 0;
}

LUA_FUNCTION_STATIC( GetName )
{
	LUA->PushString( Get( state, 1 )->GetName( ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetName )
{
	UserData *udata = GetUserdata( state, 1 );
	ConCommand *command = udata->cmd;
	if( command == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->name, LUA->CheckString( 2 ), sizeof( udata->name ) );
	command->m_pszName = udata->name;

	return 0;
}

LUA_FUNCTION_STATIC( SetFlags )
{
	Get( state, 1 )->m_nFlags = static_cast<int32_t>( LUA->CheckNumber( 2 ) );
	return 0;
}

LUA_FUNCTION_STATIC( GetFlags )
{
	LUA->PushNumber( Get( state, 1 )->m_nFlags );
	return 1;
}

LUA_FUNCTION_STATIC( HasFlag )
{
	LUA->Push( Get( state, 1 )->IsFlagSet( static_cast<int32_t>( LUA->CheckNumber( 2 ) ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetHelpText )
{
	UserData *udata = GetUserdata( state, 1 );
	ConCommand *command = udata->cmd;
	if( command == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->help, LUA->CheckString( 2 ), sizeof( udata->help ) );
	command->m_pszHelpString = udata->help;

	return 0;
}

LUA_FUNCTION_STATIC( GetHelpText )
{
	LUA->PushString( Get( state, 1 )->GetHelpText( ) );
	return 1;
}

LUA_FUNCTION_STATIC( Remove )
{
	CheckType( state, 1 );
	global::icvar->UnregisterConCommand( Destroy( state, 1 ) );
	return 0;
}

static void Initialize( lua_State *state )
{
	LUA->CreateTable( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );

	LUA->CreateMetaTableType( metaname, metatype );

	LUA->PushCFunction( gc );
	LUA->SetField( -2, "__gc" );

	LUA->PushCFunction( tostring );
	LUA->SetField( -2, "__tostring" );

	LUA->PushCFunction( eq );
	LUA->SetField( -2, "__eq" );

	LUA->PushCFunction( index );
	LUA->SetField( -2, "__index" );

	LUA->PushCFunction( newindex );
	LUA->SetField( -2, "__newindex" );

	LUA->PushCFunction( GetName );
	LUA->SetField( -2, "GetName" );

	LUA->PushCFunction( SetName );
	LUA->SetField( -2, "SetName" );

	LUA->PushCFunction( SetFlags );
	LUA->SetField( -2, "SetFlags" );

	LUA->PushCFunction( GetFlags );
	LUA->SetField( -2, "GetFlags" );

	LUA->PushCFunction( HasFlag );
	LUA->SetField( -2, "HasFlag" );

	LUA->PushCFunction( SetHelpText );
	LUA->SetField( -2, "SetHelpText" );

	LUA->PushCFunction( GetHelpText );
	LUA->SetField( -2, "GetHelpText" );

	LUA->PushCFunction( Remove );
	LUA->SetField( -2, "Remove" );

	LUA->Pop( 1 );
}

static void Deinitialize( lua_State *state )
{
	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, metaname );

	LUA->PushNil( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );
}

}

namespace concommands
{

LUA_FUNCTION_STATIC( Exists )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	ConCommand *command = global::icvar->FindCommand( LUA->GetString( 1 ) );
	if( command == nullptr )
		LUA->PushBool( false );
	else
		LUA->PushBool( command->IsCommand( ) );

	return 1;
}

LUA_FUNCTION_STATIC( GetAll )
{
	LUA->CreateTable( );

	size_t i = 0;
	ICvar::Iterator iter( global::icvar ); 
	for( iter.SetFirst( ); iter.IsValid( ); iter.Next( ) )
	{  
		ConCommand *cmd = static_cast<ConCommand *>( iter.Get( ) );
		if( cmd->IsCommand( ) )
		{
			concommand::Push( state, cmd );
			LUA->PushNumber( ++i );
			LUA->SetTable( -3 );
		}
	}

	return 1;
}

LUA_FUNCTION_STATIC( Get )
{
	concommand::Push( state, global::icvar->FindCommand( LUA->CheckString( 1 ) ) );
	return 1;
}

#if defined CONCOMMANDX_SERVER

LUA_FUNCTION_STATIC( Execute )
{
	global::ivengine->ServerCommand( LUA->CheckString( 1 ) );
	return 0;
}

#elif defined CONCOMMANDX_CLIENT

LUA_FUNCTION_STATIC( Execute )
{
	if( LUA->IsType( 2, GarrysMod::Lua::Type::BOOL ) && LUA->GetBool( 2 ) )
		global::ivengine->ClientCmd_Unrestricted( LUA->CheckString( 1 ) );
	else
		global::ivengine->ClientCmd( LUA->CheckString( 1 ) );

	return 0;
}

LUA_FUNCTION_STATIC( ExecuteOnServer )
{
	global::ivengine->ServerCmd( LUA->CheckString( 1 ) );
	return 0;
}

#endif

static void Initialize( lua_State *state )
{
	LUA->GetField( GarrysMod::Lua::INDEX_GLOBAL, "concommand" );

	LUA->PushCFunction( Exists );
	LUA->SetField( -2, "Exists" );

	LUA->PushCFunction( GetAll );
	LUA->SetField( -2, "GetAll" );

	LUA->PushCFunction( Get );
	LUA->SetField( -2, "Get" );

	LUA->PushCFunction( Execute );
	LUA->SetField( -2, "Execute" );

#if defined CONCOMMANDX_CLIENT

	LUA->PushCFunction( ExecuteOnServer );
	LUA->SetField( -2, "ExecuteOnServer" );

#endif

	LUA->Pop( 1 );
}

static void Deinitialize( lua_State *state )
{
	LUA->GetField( GarrysMod::Lua::INDEX_GLOBAL, "concommand" );

	LUA->PushNil( );
	LUA->SetField( -2, "Exists" );

	LUA->PushNil( );
	LUA->SetField( -2, "GetAll" );

	LUA->PushNil( );
	LUA->SetField( -2, "Get" );

	LUA->PushNil( );
	LUA->SetField( -2, "Execute" );

#if defined CONCOMMANDX_CLIENT

	LUA->PushNil( );
	LUA->SetField( -2, "ExecuteOnServer" );

#endif

	LUA->Pop( 1 );
}

}

#if defined CONCOMMANDX_SERVER

namespace Player
{

static const char *invalid_error = "Player object is not valid";

inline int32_t GetEntityIndex( lua_State *state, int32_t i )
{
	LUA->Push( i );
	LUA->GetField( -1, "EntIndex" );
	LUA->Push( -2 );
	LUA->Call( 1, 1 );

	return static_cast<int32_t>( LUA->GetNumber( -1 ) );
}

LUA_FUNCTION_STATIC( Command )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::ENTITY );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	edict_t *edict = global::ivengine->PEntityOfEntIndex( GetEntityIndex( state, 1 ) );
	if( edict == nullptr )
		LUA->ThrowError( invalid_error );

	global::ivengine->ClientCommand( edict, "%s", LUA->GetString( 2 ) );
	return 0;
}

static void Initialize( lua_State *state )
{
	LUA->CreateMetaTableType( "Player", GarrysMod::Lua::Type::ENTITY );

	LUA->PushCFunction( Command );
	LUA->SetField( -2, "Command" );

	LUA->Pop( 1 );
}

static void Deinitialize( lua_State *state )
{
	LUA->CreateMetaTableType( "Player", GarrysMod::Lua::Type::ENTITY );

	LUA->PushNil( );
	LUA->SetField( -2, "Command" );

	LUA->Pop( 1 );
}

}

#endif

GMOD_MODULE_OPEN( )
{
	global::Initialize( state );
	concommands::Initialize( state );
	concommand::Initialize( state );

#if defined CONCOMMANDX_SERVER

	Player::Initialize( state );

#endif

	return 0;
}

GMOD_MODULE_CLOSE( )
{

#if defined CONCOMMANDX_SERVER

	Player::Deinitialize( state );

#endif

	concommands::Deinitialize( state );
	concommand::Deinitialize( state );
	return 0;
}
