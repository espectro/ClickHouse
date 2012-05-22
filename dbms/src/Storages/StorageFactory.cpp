#include <DB/Parsers/ASTCreateQuery.h>
#include <DB/Parsers/ASTIdentifier.h>

#include <DB/Interpreters/Context.h>

#include <DB/Storages/StorageLog.h>
#include <DB/Storages/StorageMemory.h>
#include <DB/Storages/StorageDistributed.h>
#include <DB/Storages/StorageSystemNumbers.h>
#include <DB/Storages/StorageSystemOne.h>
#include <DB/Storages/StorageFactory.h>


namespace DB
{


StoragePtr StorageFactory::get(
	const String & name,
	const String & data_path,
	const String & table_name,
	Context & context,
	ASTPtr & query,
	NamesAndTypesListPtr columns) const
{
	if (name == "Log")
	{
		return new StorageLog(data_path, table_name, columns);
	}
	else if (name == "Memory")
	{
		return new StorageMemory(table_name, columns);
	}
	else if (name == "Distributed")
	{
		/** В запросе в качестве аргумента для движка указано имя конфигурационной секции,
		  *  в которой задан список удалённых серверов.
		  */
		ASTs & args = dynamic_cast<ASTFunction &>(*dynamic_cast<ASTCreateQuery &>(*query).storage).children;
		if (args.size() != 3)
			throw Exception("Storage Distributed requires exactly 3 parameters"
				" - name of configuration section with list of remote servers, name of remote database and name of remote table.",
				ErrorCodes::NUMBER_OF_ARGUMENTS_DOESNT_MATCH);
		
		String config_name 		= dynamic_cast<ASTIdentifier &>(*args[0]).name;
		String remote_database 	= dynamic_cast<ASTIdentifier &>(*args[1]).name;
		String remote_table 	= dynamic_cast<ASTIdentifier &>(*args[2]).name;
		
		StorageDistributed::Addresses addresses;

		Poco::Util::AbstractConfiguration & config = Poco::Util::Application::instance().config();
		Poco::Util::AbstractConfiguration::Keys config_keys;
		config.keys("remote_servers." + config_name, config_keys);
			
		for (Poco::Util::AbstractConfiguration::Keys::const_iterator it = config_keys.begin(); it != config_keys.end(); ++it)
			addresses.push_back(Poco::Net::SocketAddress(config.getString(*it + ".host"), config.getInt(*it + ".port")));
		
		return new StorageDistributed(table_name, columns, addresses, remote_database, remote_table, *context.data_type_factory);
	}
	else if (name == "SystemNumbers")
	{
		if (columns->size() != 1 || columns->begin()->first != "number" || columns->begin()->second->getName() != "UInt64")
			throw Exception("Storage SystemNumbers only allows one column with name 'number' and type 'UInt64'",
				ErrorCodes::ILLEGAL_COLUMN);

		return new StorageSystemNumbers(table_name);
	}
	else if (name == "SystemOne")
	{
		if (columns->size() != 1 || columns->begin()->first != "dummy" || columns->begin()->second->getName() != "UInt8")
			throw Exception("Storage SystemOne only allows one column with name 'dummy' and type 'UInt8'",
				ErrorCodes::ILLEGAL_COLUMN);

		return new StorageSystemOne(table_name);
	}
	else
		throw Exception("Unknown storage " + name, ErrorCodes::UNKNOWN_STORAGE);
}


}
