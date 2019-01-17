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

static void Initialize( GarrysMod::Lua::ILuaBase *LUA )
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

struct Container
{
	ConCommand *cmd;
	const char *name_original;
	char name[64];
	const char *help_original;
	char help[256];
};

static const char *metaname = "concommand";
static int32_t metatype = -1;
static const char *invalid_error = "invalid concommand";
static const char *table_name = "concommands_objects";

inline void CheckType( GarrysMod::Lua::ILuaBase *LUA, int32_t index )
{
	if( !LUA->IsType( index, metatype ) )
		LUA->TypeError( index, metaname );
}

inline Container *GetUserdata( GarrysMod::Lua::ILuaBase *LUA, int32_t index )
{
	return LUA->GetUserType<Container>( index, metatype );
}

static ConCommand *Get( GarrysMod::Lua::ILuaBase *LUA, int32_t index )
{
	CheckType( LUA, index );
	ConCommand *command = LUA->GetUserType<Container>( index, metatype )->cmd;
	if( command == nullptr )
		LUA->ArgError( index, invalid_error );

	return command;
}

inline void Push( GarrysMod::Lua::ILuaBase *LUA, ConCommand *command )
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

	Container *udata = LUA->NewUserType<Container>( metatype );
	udata->cmd = command;
	udata->name_original = command->m_pszName;
	udata->help_original = command->m_pszHelpString;

	LUA->PushMetaTable( metatype );
	LUA->SetMetaTable( -2 );

	LUA->CreateTable( );
	LUA->SetFEnv( -2 );

	LUA->PushUserdata( command );
	LUA->Push( -2 );
	LUA->SetTable( -4 );
	LUA->Remove( -2 );
}

inline ConCommand *Destroy( GarrysMod::Lua::ILuaBase *LUA, int32_t index )
{
	Container *udata = GetUserdata( LUA, 1 );
	ConCommand *command = udata->cmd;
	if( command == nullptr )
		return nullptr;

	LUA->GetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );
	LUA->PushUserdata( command );
	LUA->PushNil( );
	LUA->SetTable( -3 );
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

	Destroy( LUA, 1 );
	return 0;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->PushBool( Get( LUA, 1 ) == Get( LUA, 2 ) );
	return 1;
}

LUA_FUNCTION_STATIC( tostring )
{
	LUA->PushFormattedString( "%s: %p", metaname, Get( LUA, 1 ) );
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

	LUA->GetFEnv( 1 );
	LUA->Push( 2 );
	LUA->RawGet( -2 );
	return 1;
}

LUA_FUNCTION_STATIC( newindex )
{
	LUA->GetFEnv( 1 );
	LUA->Push( 2 );
	LUA->Push( 3 );
	LUA->RawSet( -3 );
	return 0;
}

LUA_FUNCTION_STATIC( GetName )
{
	LUA->PushString( Get( LUA, 1 )->GetName( ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetName )
{
	Container *udata = GetUserdata( LUA, 1 );
	ConCommand *command = udata->cmd;
	if( command == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->name, LUA->CheckString( 2 ), sizeof( udata->name ) );
	command->m_pszName = udata->name;

	return 0;
}

LUA_FUNCTION_STATIC( SetFlags )
{
	Get( LUA, 1 )->m_nFlags = static_cast<int32_t>( LUA->CheckNumber( 2 ) );
	return 0;
}

LUA_FUNCTION_STATIC( GetFlags )
{
	LUA->PushNumber( Get( LUA, 1 )->m_nFlags );
	return 1;
}

LUA_FUNCTION_STATIC( HasFlag )
{
	LUA->Push( Get( LUA, 1 )->IsFlagSet( static_cast<int32_t>( LUA->CheckNumber( 2 ) ) ) );
	return 1;
}

LUA_FUNCTION_STATIC( SetHelpText )
{
	Container *udata = GetUserdata( LUA, 1 );
	ConCommand *command = udata->cmd;
	if( command == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->help, LUA->CheckString( 2 ), sizeof( udata->help ) );
	command->m_pszHelpString = udata->help;

	return 0;
}

LUA_FUNCTION_STATIC( GetHelpText )
{
	LUA->PushString( Get( LUA, 1 )->GetHelpText( ) );
	return 1;
}

LUA_FUNCTION_STATIC( Remove )
{
	CheckType( LUA, 1 );
	global::icvar->UnregisterConCommand( Destroy( LUA, 1 ) );
	return 0;
}

static void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->CreateTable( );
	LUA->SetField( GarrysMod::Lua::INDEX_REGISTRY, table_name );

	metatype = LUA->CreateMetaTable( metaname );

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

static void Deinitialize( GarrysMod::Lua::ILuaBase *LUA )
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
			concommand::Push( LUA, cmd );
			LUA->PushNumber( ++i );
			LUA->SetTable( -3 );
		}
	}

	return 1;
}

LUA_FUNCTION_STATIC( Get )
{
	concommand::Push( LUA, global::icvar->FindCommand( LUA->CheckString( 1 ) ) );
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

static void Initialize( GarrysMod::Lua::ILuaBase *LUA )
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

static void Deinitialize( GarrysMod::Lua::ILuaBase *LUA )
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

inline int32_t GetEntityIndex( GarrysMod::Lua::ILuaBase *LUA, int32_t i )
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

	edict_t *edict = global::ivengine->PEntityOfEntIndex( GetEntityIndex( LUA, 1 ) );
	if( edict == nullptr )
		LUA->ThrowError( invalid_error );

	global::ivengine->ClientCommand( edict, "%s", LUA->GetString( 2 ) );
	return 0;
}

static void Initialize( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->GetField( GarrysMod::Lua::INDEX_REGISTRY, "Player" );

	LUA->PushCFunction( Command );
	LUA->SetField( -2, "Command" );

	LUA->Pop( 1 );
}

static void Deinitialize( GarrysMod::Lua::ILuaBase *LUA )
{
	LUA->GetField( GarrysMod::Lua::INDEX_REGISTRY, "Player" );

	LUA->PushNil( );
	LUA->SetField( -2, "Command" );

	LUA->Pop( 1 );
}

}

#endif

GMOD_MODULE_OPEN( )
{
	global::Initialize( LUA );
	concommands::Initialize( LUA );
	concommand::Initialize( LUA );

#if defined CONCOMMANDX_SERVER

	Player::Initialize( LUA );

#endif

	return 0;
}

GMOD_MODULE_CLOSE( )
{

#if defined CONCOMMANDX_SERVER

	Player::Deinitialize( LUA );

#endif

	concommands::Deinitialize( LUA );
	concommand::Deinitialize( LUA );
	return 0;
}
