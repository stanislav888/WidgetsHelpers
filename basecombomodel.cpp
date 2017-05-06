#include "basecombomodel.h"
#include "addressbookmainwindow.h"
#include <QSqlQuery>

namespace
{
	enum Columns // Depends with 'query.prepare( QString( "SELECT ... '
	{
		Id,
		Data,
	};
}

BaseComboModel::BaseComboModel( const QString& visualColumn,  QObject *parent ) :
	QSqlQueryModel( parent )
{
	QSqlQuery query;
	query.prepare( QString( "SELECT id, %1 FROM country" ).arg( visualColumn ) );
	EXEC_SQL_QUERY( query );
	QSqlQueryModel::setQuery( query );
}

QVariant BaseComboModel::dataFromParent( QModelIndex index, int column ) const
{
	return QSqlQueryModel::data( QSqlQueryModel::index( index.row() - 1, column ) );
}

QVariant BaseComboModel::data(const QModelIndex & item, int role /* = Qt::DisplayRole */) const
{
	QVariant result;

	if( item.row() == 0 && role == Qt::UserRole ) // Make first row empty
	{
		result = 0;
	}
	else if( item.row() > 0 )
	{
		switch( role )
		{
			case Qt::UserRole:
				result = dataFromParent( item, Id );
				break;
			case Qt::DisplayRole:
				result = dataFromParent( item, Data );
				break;
			default:
				break;
		}
	}

	return result;
}
