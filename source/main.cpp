#include <GarrysMod/Lua/Interface.h>
#include <cstdint>
#include <interface.h>
#include <hackedconvar.h>
#include <inetchannel.h>
#include <eiface.h>
#include <cdll_int.h>

namespace global
{

#if defined _WIN32

static CDllDemandLoader icvar_loader( "vstdlib.dll" );
static CDllDemandLoader engine_loader( "engine.dll" );

#elif defined __linux

#if defined CONCOMMANDX_SERVER

static CDllDemandLoader icvar_loader( "libvstdlib_srv.so" );
static CDllDemandLoader engine_loader( "engine_srv.so" );

#elif defined CONCOMMANDX_CLIENT

static CDllDemandLoader icvar_loader( "libvstdlib.so" );
static CDllDemandLoader engine_loader( "engine.so" );

#endif

#elif defined __APPLE__

static CDllDemandLoader icvar_loader( "libvstdlib.dylib" );
static CDllDemandLoader engine_loader( "engine.dylib" );

#endif

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
	CreateInterfaceFn factory = icvar_loader.GetFactory( );
	if( factory == nullptr )
		LUA->ThrowError( "Couldn't get vstdlib factory. Critical error." );

	icvar = static_cast<ICvar *>( factory( CVAR_INTERFACE_VERSION, nullptr ) );
	if( icvar == nullptr )
		LUA->ThrowError( "ICVar not initialized. Critical error." );

	factory = engine_loader.GetFactory( );
	if( factory == nullptr )
		LUA->ThrowError( "Couldn't get engine factory. Critical error." );

	ivengine = static_cast<IVEngine *>( factory( ivengine_name, nullptr ) );
	if( ivengine == nullptr )
		LUA->ThrowError( "IVEngineServer/Client not initialized. Critical error." );
}

}

namespace concommand
{

struct userdata
{
	void *data;
	uint8_t type;
	char name[64];
	char help[256];
};

static const char *metaname = "concommand";
static uint8_t metatype = GarrysMod::Lua::Type::COUNT + 2;

static const char *invalid_error = "concommand object is not valid";

static userdata *Create( lua_State *state )
{
	userdata *udata = static_cast<userdata *>( LUA->NewUserdata( sizeof( userdata ) ) );
	udata->type = metatype;

	LUA->CreateMetaTableType( metaname, metatype );
	LUA->SetMetaTable( -2 );

	return udata;
}

inline userdata *GetUserdata( lua_State *state, int index )
{
	return static_cast<userdata *>( LUA->GetUserdata( index ) );
}

inline ConCommand *GetAndValidate( lua_State *state, int index, const char *err )
{
	ConCommand *cmd = static_cast<ConCommand *>( GetUserdata( state, index )->data );
	if( cmd == nullptr )
		LUA->ThrowError( err );

	return cmd;
}

LUA_FUNCTION_STATIC( gc )
{
	LUA->CheckType( 1, metatype );

	userdata *udata = static_cast<userdata *>( LUA->GetUserdata( 1 ) );
	if( udata->data == nullptr )
		return 0;

	udata->data = nullptr;

	return 0;
}

LUA_FUNCTION_STATIC( eq )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, metatype );

	ConCommand *cmd1 = GetAndValidate( state, 1, invalid_error );
	ConCommand *cmd2 = GetAndValidate( state, 2, invalid_error );

	LUA->PushBool( cmd1 == cmd2 );

	return 1;
}

LUA_FUNCTION_STATIC( tostring )
{
	LUA->CheckType( 1, metatype );

	ConCommand *cmd = GetAndValidate( state, 1, invalid_error );

	char formatted[30] = { 0 };
	V_snprintf( formatted, sizeof( formatted ), "%s: 0x%p", metaname, cmd );
	LUA->PushString( formatted );

	return 1;
}

LUA_FUNCTION_STATIC( GetName )
{
	LUA->CheckType( 1, metatype );

	ConCommand *cmd = GetAndValidate( state, 1, invalid_error );

	LUA->PushString( cmd->GetName( ) );

	return 1;
}

LUA_FUNCTION_STATIC( SetName )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	userdata *udata = static_cast<userdata *>( LUA->GetUserdata( 1 ) );

	ConCommand *cmd = static_cast<ConCommand *>( udata->data );
	if( cmd == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->name, LUA->GetString( 2 ), sizeof( udata->name ) );
	cmd->m_pszName = udata->name;

	return 0;
}

LUA_FUNCTION_STATIC( SetFlags )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	ConCommand *cmd = GetAndValidate( state, 1, invalid_error );

	cmd->m_nFlags = static_cast<int>( LUA->GetNumber( 2 ) );

	return 0;
}

LUA_FUNCTION_STATIC( GetFlags )
{
	LUA->CheckType( 1, metatype );

	ConCommand *cmd = GetAndValidate( state, 1, invalid_error );

	LUA->PushNumber( cmd->m_nFlags );

	return 1;
}

LUA_FUNCTION_STATIC( HasFlag )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::NUMBER );

	ConCommand *cmd = GetAndValidate( state, 1, invalid_error );

	LUA->Push( cmd->IsFlagSet( static_cast<int>( LUA->GetNumber( 2 ) ) ) );

	return 1;
}

LUA_FUNCTION_STATIC( SetHelpText )
{
	LUA->CheckType( 1, metatype );
	LUA->CheckType( 2, GarrysMod::Lua::Type::STRING );

	userdata *udata = static_cast<userdata *>( LUA->GetUserdata( 1 ) );

	ConCommand *cmd = static_cast<ConCommand *>( udata->data );
	if( cmd == nullptr )
		LUA->ThrowError( invalid_error );

	V_strncpy( udata->help, LUA->GetString( 2 ), sizeof( udata->help ) );
	cmd->m_pszHelpString = udata->help;

	return 0;
}

LUA_FUNCTION_STATIC( GetHelpText )
{
	LUA->CheckType( 1, metatype );

	ConCommand *cmd = GetAndValidate( state, 1, invalid_error );

	LUA->PushString( cmd->GetHelpText( ) );

	return 1;
}

LUA_FUNCTION_STATIC( Remove )
{
	LUA->CheckType( 1, metatype );

	ConCommand *cmd = GetAndValidate( state, 1, invalid_error );

	global::icvar->UnregisterConCommand( cmd );

	return 0;
}

static void RegisterMetaTable( lua_State *state )
{
	LUA->CreateMetaTableType( metaname, metatype );

	LUA->PushCFunction( gc );
	LUA->SetField( -2, "__gc" );

	LUA->PushCFunction( tostring );
	LUA->SetField( -2, "__tostring" );

	LUA->PushCFunction( eq );
	LUA->SetField( -2, "__eq" );

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

	LUA->Push( -1 );
	LUA->SetField( -2, "__index" );
}

}

namespace concommands
{

LUA_FUNCTION_STATIC( Exists )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	ConCommand *cvar = global::icvar->FindCommand( LUA->GetString( 1 ) );
	if( cvar == nullptr )
		LUA->PushBool( false );
	else
		LUA->PushBool( cvar->IsCommand( ) );

	return 1;
}

LUA_FUNCTION_STATIC( GetAll )
{
	LUA->CreateTable( );

	size_t i = 0;
	ICvar::Iterator iter( global::icvar ); 
	for( iter.SetFirst( ); iter.IsValid( ); iter.Next( ) )
	{  
		ConCommandBase *cmd = iter.Get( );
		if( cmd->IsCommand( ) )
		{
			concommand::Create( state )->data = cmd;
			LUA->PushNumber( ++i );
			LUA->SetTable( -3 );
		}
	}

	return 1;
}

LUA_FUNCTION_STATIC( Get )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	ConCommand *cmd = global::icvar->FindCommand( LUA->GetString( 1 ) );
	if( cmd == nullptr )
		return 0;

	concommand::Create( state )->data = cmd;
	return 1;
}

#if defined CONCOMMANDX_SERVER

LUA_FUNCTION_STATIC( Execute )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	global::ivengine->ServerCommand( LUA->GetString( 1 ) );

	return 0;
}

#elif defined CONCOMMANDX_CLIENT

LUA_FUNCTION_STATIC( Execute )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	if( LUA->IsType( 2, GarrysMod::Lua::Type::BOOL ) && LUA->GetBool( 2 ) )
		global::ivengine->ClientCmd_Unrestricted( LUA->GetString( 1 ) );
	else
		global::ivengine->ClientCmd( LUA->GetString( 1 ) );

	return 0;
}

LUA_FUNCTION_STATIC( ExecuteOnServer )
{
	LUA->CheckType( 1, GarrysMod::Lua::Type::STRING );

	global::ivengine->ServerCmd( LUA->GetString( 1 ) );

	return 0;
}

#endif

static void RegisterGlobalTable( lua_State *state )
{
	LUA->PushSpecial( GarrysMod::Lua::SPECIAL_GLOB );

	LUA->GetField( -1, "concommand" );

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

}

}

namespace Player
{

#if defined CONCOMMANDX_SERVER

static const char *invalid_error = "Player object is not valid";

inline int GetEntityIndex( lua_State *state, int i )
{
	LUA->Push( i );
	LUA->GetField( -1, "EntIndex" );
	LUA->Push( -2 );
	LUA->Call( 1, 1 );

	return static_cast<int>( LUA->GetNumber( -1 ) );
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

#endif

static void RegisterMetaTable( lua_State *state )
{

#if defined CONCOMMANDX_SERVER

	LUA->CreateMetaTableType( "Player", GarrysMod::Lua::Type::ENTITY );

	LUA->PushCFunction( Command );
	LUA->SetField( -2, "Command" );

#endif

}

}

GMOD_MODULE_OPEN( )
{
	global::Initialize( state );

	concommands::RegisterGlobalTable( state );

	concommand::RegisterMetaTable( state );

	Player::RegisterMetaTable( state );

	return 0;
}

GMOD_MODULE_CLOSE( )
{
	(void)state;
	return 0;
}