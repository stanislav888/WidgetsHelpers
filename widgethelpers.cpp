#include "widgethelpers.h"
#include <QVariant>
#include <QWidget>
#include <QSqlQuery>
#include <QSqlError>
#include <QApplication>
#include <QStyle>
#include <QSqlTableModel>
#include <QSortFilterProxyModel>
#include <QTableView>
#include <QPushButton>
#include <QMetaObject>
#include <QMetaProperty>

#include <QRadioButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QCheckBox>
#include <QDateEdit>
#include <QLabel>

#ifndef Q_NULLPTR
#define Q_NULLPTR NULL
#endif

namespace
{
	const char*	VALUE_PROP = "v";

	QByteArray userPropertyName( const QObject* const inputBox )
	{
		return inputBox->metaObject()->userProperty().name();
	}
}

const char*	WidgetHelpers::COLUMN_NAME_PROP = "c";
const char* WidgetHelpers::TABLE_NAME_PROP = "t";

QSqlRecord WidgetHelpers::addRecord( const QString& tableName, const QVariant& recordId )
{
	QSqlQuery query;
	query.prepare( QString( "SELECT * FROM %1 WHERE id = :id " ).arg( tableName ) );
	query.bindValue( ":id", recordId );
	EXEC_SQL_QUERY( query );
	query.first();
	QSqlRecord record = query.record();
	m_tablesRecords.insert( tableName, record );

	return record;
}

void WidgetHelpers::setupForm( QWidget* formPointer, const QString& defaultTableName )
{
	if( m_defaultTableName.isEmpty() )
		m_defaultTableName = defaultTableName;

	const QMetaMethod saveDataSl = metaObject()->method(
		metaObject()->indexOfSlot( "saveDataSlot()" ) );

	foreach( QWidget* w, formPointer->findChildren< QWidget * >() )
	{
		const QVariant columnName( w->property( COLUMN_NAME_PROP ) );

		if( columnName.isNull() )
			continue; // Not need to set value

		m_inputsList << w;
		QString tablePropStr( w->property( TABLE_NAME_PROP ).toString() );

		if( tablePropStr.isEmpty() && columnName.isValid() ) // In case of defailt table
		{
			tablePropStr = defaultTableName;
			w->setProperty( TABLE_NAME_PROP, defaultTableName ); // Assign default
		}

		const QMetaMethod signal = w->metaObject()->userProperty().notifySignal();

		if( QRadioButton* const button = qobject_cast< QRadioButton* >( w ) )
		{
			QButtonGroup* grp = button->group();
			Q_ASSERT( !"No group for that button" || grp );
			grp->setProperty( COLUMN_NAME_PROP, button->property( COLUMN_NAME_PROP ) ); // For saveDataSlot()
			grp->setProperty( TABLE_NAME_PROP, button->property( TABLE_NAME_PROP ) );
			connect( grp, SIGNAL( buttonClicked( QAbstractButton* ) ), SLOT( saveDataSlot() ) );
		}
#if( QT_VERSION < QT_VERSION_CHECK(5, 0, 0) ) // Just different NOTIFY attribute in different Qt versions. Not sure with version crireria exactly
		else if( QComboBox* const box = qobject_cast< QComboBox* >( w ) )
		{
			connect( box, SIGNAL( currentIndexChanged( int ) ), SLOT( saveDataSlot() ) );
		}
#endif
		else if( signal.enclosingMetaObject() != Q_NULLPTR )
		{
			connect( w, signal, this, saveDataSl );
		}
		else if( QLabel* const box = qobject_cast< QLabel* >( w ) )
		{
			Q_UNUSED( box ); // No need save data from QLabel
		}
		else
			Q_ASSERT( !"Please implement that widjet type!" );
	}
}

void WidgetHelpers::fillForm(const QVariant& tableRecordId, const QString &tableName /*= QString::null */)
{
	const bool setFormEmpty = !tableRecordId.isValid();
	const QString currentTable( tableName.isEmpty() ? m_defaultTableName : tableName );

	if( setFormEmpty )
		m_tablesRecords.clear();
	else
		addRecord( currentTable, tableRecordId );

	foreach( QWidget* w, m_inputsList )
	{
		const QString tablePropStr( w->property( TABLE_NAME_PROP ).toString() );
		const QString columnName( w->property( COLUMN_NAME_PROP ).toString() );
		const QSqlRecord& record = m_tablesRecords.value( tablePropStr );

		if( QString::compare( currentTable, tablePropStr, Qt::CaseInsensitive ) == 0 )
		{

#ifdef QT_DEBUG

			const char* ORIGINAL_TOOLTIP_PROP = "original_tooltip"; // No difference what name
			const char* DEBUG_TOOLTIP_PREFIX = "t = "; // Prefix visible on debug tooltip

			QString debugToolTip
				, widgetToolTip( w->toolTip() )
				, originalToolTip( w->property( ORIGINAL_TOOLTIP_PROP ).toString() );

			if( !setFormEmpty )
			{
				debugToolTip = "%1, c = %2, id = %3";
				debugToolTip = debugToolTip.arg( DEBUG_TOOLTIP_PREFIX + tablePropStr ).arg( columnName ).arg( record.value( record.indexOf( "id" ) ).toString() );
			}

			if( !widgetToolTip.startsWith( DEBUG_TOOLTIP_PREFIX ) && !widgetToolTip.isEmpty() && originalToolTip.isEmpty() )
			{
				w->setProperty( ORIGINAL_TOOLTIP_PROP, widgetToolTip );
				originalToolTip = widgetToolTip;
			}

			if( !originalToolTip.isEmpty() && !debugToolTip.isEmpty() )
				originalToolTip += "<br/> -------------------- <br/>";

			w->setToolTip( originalToolTip + debugToolTip );

#endif

			QVariant value;

			if( setFormEmpty )
				value = QVariant( QString::null );
			else
				value = record.value( record.indexOf( columnName ) );

			setWidgetValue( value, w ); // In some cases can change focus at setVidgetValue()

			if( QAbstractButton* btn = qobject_cast< QAbstractButton* >( w ) )
			{
				QButtonGroup* grp = btn->group();

				if( grp )
				{
					foreach( QAbstractButton* b, grp->buttons() )
					{
						b->setDisabled( setFormEmpty );
					}
				}
			}

			w->setDisabled( setFormEmpty );

		}
	}

	if( currentTable == m_defaultTableName ) // Need that only one time. When handle default table
	{
		foreach( QWidget* w, m_additionalWidgetsToDisable )
		{
			w->setDisabled( setFormEmpty );
		}
	}
}

void WidgetHelpers::setAdditionalDisableWidgets( const QWidgetList& widgets )
{
	m_additionalWidgetsToDisable = widgets;
}

void WidgetHelpers::saveDataSlot()
{
	const QObject* const sendr = sender();

	if( sendr )
	{
		const QString tableName( sendr->property( TABLE_NAME_PROP ).toString() )
				, columnName( sendr->property( COLUMN_NAME_PROP ).toString() );

		const QVariant newValue( getWidgetValue( sendr ) ),
						id( getTableId( tableName ) ),
						oldValue( getFieldValue( tableName, columnName ) );

		if( !tableName.isEmpty() && !columnName.isEmpty()
				&& newValue != oldValue ) // Prevent old data saving
		{
			QSqlQuery updateQuery;
			updateQuery.prepare( QString( "UPDATE %1 SET %2 = :value WHERE id = :id" ).arg( tableName ).arg( columnName ) );
			updateQuery.bindValue( ":value", newValue );
			updateQuery.bindValue( ":id", id );
			EXEC_SQL_QUERY( updateQuery );
			emit dataChanged( tableName, columnName, id, newValue );
		}
	}
	else
		Q_ASSERT( !"Please use as SLOT only!" );
}

void WidgetHelpers::setWidgetValue( const QVariant &value, QObject* const inputBox )
{
	if( inputBox == Q_NULLPTR )
		return;

	inputBox->blockSignals( true ); // Prevent unwanted data saving
	{
		const QByteArray uPropName = userPropertyName( inputBox );
		QDateEdit* dateEdit = qobject_cast< QDateEdit* >( inputBox );

		if( dateEdit && value.isNull() )
		{
			dateEdit->setDate( dateEdit->minimumDate() );
		}
		else if( QComboBox *box = qobject_cast< QComboBox* >( inputBox ) )
		{
			box->setDisabled( true ); // Prevent chaning selection at personsTable
			const int index = box->findData( value, Qt::UserRole, Qt::MatchExactly );

			if( index > -1 )
				box->setCurrentIndex( index );

			box->setDisabled( false );
		}
		else if( QRadioButton *box = qobject_cast< QRadioButton* >( inputBox ) )
			setWidgetValue( value, box->group() );
		else if( !uPropName.isEmpty() )
			inputBox->setProperty( uPropName, value );
		else if( QButtonGroup* grp = qobject_cast< QButtonGroup* >( inputBox ) )
		{
			foreach( QAbstractButton* abtn, grp->buttons() ) // QRadioButton in common case
			{
				if( abtn )
				{
					if( abtn->property( VALUE_PROP ) == value )
					{
						abtn->setChecked( true );
						break;
					}
					else
						abtn->setChecked( false );
				}
			}
		}
		else if( QLabel* box = qobject_cast< QLabel* >( inputBox ) )
			box->setText( value.toString() );
		else
		{
			qDebug() << inputBox->metaObject()->className();
			Q_ASSERT( !"Please implement that widget type!" );
		}
	}

	inputBox->blockSignals( false );
}

QVariant WidgetHelpers::getWidgetValue( const QObject* const sourceBox )
{
	if( sourceBox == Q_NULLPTR )
		return QVariant();

	QVariant result;
	const QByteArray uPropName = userPropertyName( sourceBox );

	if( const QComboBox* box = qobject_cast< const QComboBox* >( sourceBox ) )
		result = box->itemData( box->currentIndex(), Qt::UserRole );
	else if( const QAbstractButton* box = qobject_cast< const QAbstractButton* >( sourceBox ) ) // Another buttons. QCheckBox handling before
		result = box->property( VALUE_PROP );
	else if( !uPropName.isEmpty() )
		result = sourceBox->property( uPropName );
	else if( const QButtonGroup* buttonGroup = qobject_cast< const QButtonGroup* >( sourceBox ) )
		result = getWidgetValue( buttonGroup->checkedButton() ); // to QAbstractButton
	else
	{
		Q_ASSERT( !"Please implement that widget type!" );
	}

	return result;
}

QVariant WidgetHelpers::getTableId( const QString& tableName ) const
{
	return getFieldValue( tableName, "id" );
}

QVariant WidgetHelpers::getFieldValue( const QString& tableName, const QString& columnName ) const
{
	const QSqlRecord& rec = m_tablesRecords.value( tableName );
	QVariant value( rec.value( columnName ) );
	return value;
}

bool WidgetHelpers::execSqlQuery( QSqlQuery *query, const QString funcInfo )
{
	bool queryOk = query->exec();

	if( !queryOk )
	{
		qDebug() << "-------"<< funcInfo << " SQL query completed with errors-------";
		qDebug() << query->lastQuery();
		qDebug() << query->lastError();
		qDebug() << "----------------------------------------------";
	}

	return queryOk;
}

int WidgetHelpers::addRowToModel( QSqlTableModel *sourceModel )
{
	int row = sourceModel->rowCount();
	bool ok = sourceModel->insertRow( row );
	ok = ok && sourceModel->setData( sourceModel->index( row, 1 /* any user data row */ ), QVariant( QString::null ) ); // Hack for save row in DB
	ok = ok && sourceModel->submit();

	return row;
}

bool WidgetHelpers::deleteRowFromModels( QSqlTableModel *sourceModel, QSortFilterProxyModel* proxyModel, QTableView* tableView )
{
	int r = selectedRow( tableView );
	bool ok = false;

	if( r != NO_SELECTION && tableView && sourceModel && proxyModel )
	{
		// Little hack that cover some Qt bugs. Column index here(0) have no difference
		tableView->hideRow( proxyModel->mapFromSource( sourceModel->index( r, 0 ) ).row() );
		ok = sourceModel->removeRow( r );
		sourceModel->submit();
	}

	return ok;
}

int WidgetHelpers::selectedRow( QTableView* tableView )
{
	int row = NO_SELECTION;

	if( tableView )
	{
		QItemSelectionModel* selectionModel = tableView->selectionModel();

		if( selectionModel )
		{
			QModelIndexList selected = selectionModel->selectedIndexes();

			// For see what kind of model class used for selection model
//			qDebug() << selected.first().model()->metaObject()->className();

			if( !selected.isEmpty() )
			{
				QModelIndex firstIndex = selected.first();
				const QSortFilterProxyModel* proxyModel = qobject_cast<  const QSortFilterProxyModel* >( firstIndex.model() );

				if( proxyModel )
					row = proxyModel->mapToSource( firstIndex ).row();
				else
					row = firstIndex.row();
			}
		}
	}

	return row;
}

