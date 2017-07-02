/***************************************************************************
    qgsdualview.cpp
     --------------------------------------
    Date                 : 10.2.2013
    Copyright            : (C) 2013 Matthias Kuhn
    Email                : matthias at opengis dot ch
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsapplication.h"
#include "qgsactionmanager.h"
#include "qgsattributetablemodel.h"
#include "qgsdualview.h"
#include "qgsexpressionbuilderdialog.h"
#include "qgsfeaturelistmodel.h"
#include "qgsifeatureselectionmanager.h"
#include "qgsmapcanvas.h"
#include "qgsmaplayeractionregistry.h"
#include "qgsmessagelog.h"
#include "qgsvectordataprovider.h"
#include "qgsvectorlayercache.h"
#include "qgsorganizetablecolumnsdialog.h"
#include "qgseditorwidgetregistry.h"
#include "qgssettings.h"
#include "qgsscrollarea.h"

#include <QClipboard>
#include <QDialog>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QGroupBox>
#include <QInputDialog>

QgsDualView::QgsDualView( QWidget *parent )
  : QStackedWidget( parent )
  , mEditorContext()
  , mMasterModel( nullptr )
  , mFilterModel( nullptr )
  , mFeatureListModel( nullptr )
  , mAttributeForm( nullptr )
  , mHorizontalHeaderMenu( nullptr )
  , mLayerCache( nullptr )
  , mProgressDlg( nullptr )
  , mFeatureSelectionManager( nullptr )
  , mAttributeEditorScrollArea( nullptr )
{
  std::cout << "QgsDualView::QgsDualView NEW!" << std::endl;
  setupUi( this );

  mConditionalFormatWidget->hide();

  mPreviewActionMapper = new QSignalMapper( this );

  mPreviewColumnsMenu = new QMenu( this );
  mActionPreviewColumnsMenu->setMenu( mPreviewColumnsMenu );

  // Set preview icon
  mActionExpressionPreview->setIcon( QgsApplication::getThemeIcon( QStringLiteral( "/mIconExpressionPreview.svg" ) ) );

  // Connect layer list preview signals
  connect( mActionExpressionPreview, &QAction::triggered, this, &QgsDualView::previewExpressionBuilder );
  connect( mPreviewActionMapper, static_cast < void ( QSignalMapper::* )( QObject * ) > ( &QSignalMapper::mapped ), this, &QgsDualView::previewColumnChanged );
  connect( mFeatureList, &QgsFeatureListView::displayExpressionChanged, this, &QgsDualView::previewExpressionChanged );
}

void QgsDualView::init( QgsVectorLayer *layer, QgsMapCanvas *mapCanvas, const QgsFeatureRequest &request, const QgsAttributeEditorContext &context, bool loadFeatures )
{
  std::cout << "QgsDualView::init" << std::endl;
  mMapCanvas = mapCanvas;

  if ( !layer )
    return;

  mLayer = layer;

  mEditorContext = context;

  connect( mTableView, &QgsAttributeTableView::willShowContextMenu, this, &QgsDualView::viewWillShowContextMenu );
  mTableView->horizontalHeader()->setContextMenuPolicy( Qt::CustomContextMenu );
  connect( mTableView->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &QgsDualView::showViewHeaderMenu );
  connect( mTableView, &QgsAttributeTableView::columnResized, this, &QgsDualView::tableColumnResized );

  initLayerCache( !( request.flags() & QgsFeatureRequest::NoGeometry ) || !request.filterRect().isNull() );
  initModels( mapCanvas, request, loadFeatures );

  mConditionalFormatWidget->setLayer( mLayer );

  mTableView->setModel( mFilterModel );
  mFeatureList->setModel( mFeatureListModel );
  delete mAttributeForm;
  mAttributeForm = new QgsAttributeForm( mLayer, QgsFeature(), mEditorContext );
  if ( !context.parentContext() )
  {
    mAttributeEditorScrollArea = new QgsScrollArea();
    mAttributeEditorScrollArea->setWidgetResizable( true );
    mAttributeEditor->layout()->addWidget( mAttributeEditorScrollArea );
    mAttributeEditorScrollArea->setWidget( mAttributeForm );
  }
  else
  {
    mAttributeEditor->layout()->addWidget( mAttributeForm );
  }

  connect( mAttributeForm, &QgsAttributeForm::attributeChanged, this, &QgsDualView::featureFormAttributeChanged );
  connect( mAttributeForm, &QgsAttributeForm::modeChanged, this, &QgsDualView::formModeChanged );
  connect( mMasterModel, &QgsAttributeTableModel::modelChanged, mAttributeForm, &QgsAttributeForm::refreshFeature );
  connect( mAttributeForm, &QgsAttributeForm::filterExpressionSet, this, &QgsDualView::filterExpressionSet );
  connect( mFilterModel, &QgsAttributeTableFilterModel::sortColumnChanged, this, &QgsDualView::onSortColumnChanged );
  if ( mFeatureListPreviewButton->defaultAction() )
    mFeatureList->setDisplayExpression( mDisplayExpression );
  else
    columnBoxInit();

  // This slows down load of the attribute table heaps and uses loads of memory.
  //mTableView->resizeColumnsToContents();

  mFeatureList->setEditSelection( QgsFeatureIds() << mFeatureListModel->idxToFid( mFeatureListModel->index( 0, 0 ) ) );
}

void QgsDualView::columnBoxInit()
{
  // load fields
  QList<QgsField> fields = mLayer->fields().toList();

  QString defaultField;

  // default expression: saved value
  QString displayExpression = mLayer->displayExpression();

  if ( displayExpression.isEmpty() )
  {
    // ... there isn't really much to display
    displayExpression = QStringLiteral( "'[Please define preview text]'" );
  }

  mFeatureListPreviewButton->addAction( mActionExpressionPreview );
  mFeatureListPreviewButton->addAction( mActionPreviewColumnsMenu );

  Q_FOREACH ( const QgsField &field, fields )
  {
    int fieldIndex = mLayer->fields().lookupField( field.name() );
    if ( fieldIndex == -1 )
      continue;

    if ( QgsEditorWidgetRegistry::instance()->findBest( mLayer, field.name() ).type() != QLatin1String( "Hidden" ) )
    {
      QIcon icon = mLayer->fields().iconForField( fieldIndex );
      QString text = field.name();

      // Generate action for the preview popup button of the feature list
      QAction *previewAction = new QAction( icon, text, mFeatureListPreviewButton );
      mPreviewActionMapper->setMapping( previewAction, previewAction );
      connect( previewAction, &QAction::triggered, this, [previewAction, this]( bool ) { this->mPreviewActionMapper->map( previewAction ); }
             );
      mPreviewColumnsMenu->addAction( previewAction );

      if ( text == defaultField )
      {
        mFeatureListPreviewButton->setDefaultAction( previewAction );
      }
    }
  }

  // If there is no single field found as preview
  if ( !mFeatureListPreviewButton->defaultAction() )
  {
    mFeatureList->setDisplayExpression( displayExpression );
    mFeatureListPreviewButton->setDefaultAction( mActionExpressionPreview );
    mDisplayExpression = mFeatureList->displayExpression();
  }
  else
  {
    mFeatureListPreviewButton->defaultAction()->trigger();
  }

  QAction *sortByPreviewExpression = new QAction( QgsApplication::getThemeIcon( QStringLiteral( "sort.svg" ) ), tr( "Sort by preview expression" ), this );
  connect( sortByPreviewExpression, &QAction::triggered, this, &QgsDualView::sortByPreviewExpression );
  mFeatureListPreviewButton->addAction( sortByPreviewExpression );
}

void QgsDualView::setView( QgsDualView::ViewMode view )
{
  setCurrentIndex( view );
}

QgsDualView::ViewMode QgsDualView::view() const
{
  return static_cast< QgsDualView::ViewMode >( currentIndex() );
}

void QgsDualView::setFilterMode( QgsAttributeTableFilterModel::FilterMode filterMode )
{
  // cleanup any existing connections
  switch ( mFilterModel->filterMode() )
  {
    case QgsAttributeTableFilterModel::ShowVisible:
      disconnect( mMapCanvas, &QgsMapCanvas::extentsChanged, this, &QgsDualView::extentChanged );
      break;

    case QgsAttributeTableFilterModel::ShowAll:
    case QgsAttributeTableFilterModel::ShowEdited:
    case QgsAttributeTableFilterModel::ShowFilteredList:
      break;

    case QgsAttributeTableFilterModel::ShowSelected:
      disconnect( masterModel()->layer(), &QgsVectorLayer::selectionChanged, this, &QgsDualView::updateSelectedFeatures );
      break;
  }

  QgsFeatureRequest r = mMasterModel->request();
  bool needsGeometry = filterMode == QgsAttributeTableFilterModel::ShowVisible;

  bool requiresTableReload = ( r.filterType() != QgsFeatureRequest::FilterNone || !r.filterRect().isNull() ) // previous request was subset
                             || ( needsGeometry && r.flags() & QgsFeatureRequest::NoGeometry ) // no geometry for last request
                             || ( mMasterModel->rowCount() == 0 ); // no features

  if ( !needsGeometry )
    r.setFlags( r.flags() | QgsFeatureRequest::NoGeometry );
  else
    r.setFlags( r.flags() & ~( QgsFeatureRequest::NoGeometry ) );
  r.setFilterFids( QgsFeatureIds() );
  r.setFilterRect( QgsRectangle() );
  r.disableFilter();

  // setup new connections and filter request parameters
  switch ( filterMode )
  {
    case QgsAttributeTableFilterModel::ShowVisible:
      connect( mMapCanvas, &QgsMapCanvas::extentsChanged, this, &QgsDualView::extentChanged );
      if ( mMapCanvas )
      {
        QgsRectangle rect = mMapCanvas->mapSettings().mapToLayerCoordinates( mLayer, mMapCanvas->extent() );
        r.setFilterRect( rect );
      }
      break;

    case QgsAttributeTableFilterModel::ShowAll:
    case QgsAttributeTableFilterModel::ShowEdited:
    case QgsAttributeTableFilterModel::ShowFilteredList:
      break;

    case QgsAttributeTableFilterModel::ShowSelected:
      connect( masterModel()->layer(), &QgsVectorLayer::selectionChanged, this, &QgsDualView::updateSelectedFeatures );
      r.setFilterFids( masterModel()->layer()->selectedFeatureIds() );
      break;
  }

  if ( requiresTableReload )
  {
    mMasterModel->setRequest( r );
    whileBlocking( mLayerCache )->setCacheGeometry( needsGeometry );
    mMasterModel->loadLayer();
  }

  //update filter model
  mFilterModel->setFilterMode( filterMode );
  emit filterChanged();
}

void QgsDualView::setSelectedOnTop( bool selectedOnTop )
{
  mFilterModel->setSelectedOnTop( selectedOnTop );
}

void QgsDualView::initLayerCache( bool cacheGeometry )
{
  std::cout << "QgsDUalView::initLayerCache 0" << std::endl;
  // Initialize the cache
  QgsSettings settings;
  int cacheSize = settings.value( QStringLiteral( "qgis/attributeTableRowCache" ), "10000" ).toInt();
  mLayerCache = new QgsVectorLayerCache( mLayer, cacheSize, this );
  std::cout << "QgsDualView::initLayerCache 00 " << mLayer->getFeature( 0 ).attribute( 1 ).toString().toStdString() << std::endl;
  mLayerCache->setCacheGeometry( cacheGeometry );
  if ( 0 == cacheSize || 0 == ( QgsVectorDataProvider::SelectAtId & mLayer->dataProvider()->capabilities() ) )
  {
    std::cout << "QgsDUalView::initLayerCache 1" << std::endl;
    connect( mLayerCache, &QgsVectorLayerCache::invalidated, this, &QgsDualView::rebuildFullLayerCache );
    rebuildFullLayerCache();
  }
}

void QgsDualView::initModels( QgsMapCanvas *mapCanvas, const QgsFeatureRequest &request, bool loadFeatures )
{
  delete mFeatureListModel;
  delete mFilterModel;
  delete mMasterModel;

  mMasterModel = new QgsAttributeTableModel( mLayerCache, this );
  mMasterModel->setRequest( request );
  mMasterModel->setEditorContext( mEditorContext );
  mMasterModel->setExtraColumns( 1 ); // Add one extra column which we can "abuse" as an action column

  connect( mMasterModel, &QgsAttributeTableModel::progress, this, &QgsDualView::progress );
  connect( mMasterModel, &QgsAttributeTableModel::finished, this, &QgsDualView::finished );

  connect( mConditionalFormatWidget, &QgsFieldConditionalFormatWidget::rulesUpdated, mMasterModel, &QgsAttributeTableModel::fieldConditionalStyleChanged );

  if ( loadFeatures )
    mMasterModel->loadLayer();

  mFilterModel = new QgsAttributeTableFilterModel( mapCanvas, mMasterModel, mMasterModel );

  connect( mFeatureList, &QgsFeatureListView::displayExpressionChanged, this, &QgsDualView::displayExpressionChanged );

  mFeatureListModel = new QgsFeatureListModel( mFilterModel, mFilterModel );
}

void QgsDualView::on_mFeatureList_aboutToChangeEditSelection( bool &ok )
{
  std::cout << "QgsDualView::on_mFeatureList_aboutToChangeEditSelection 0" << std::endl;
  if ( mLayer->isEditable() && !mAttributeForm->save() )
    ok = false;
}

void QgsDualView::on_mFeatureList_currentEditSelectionChanged( const QgsFeature &feat )
{
  std::cout << "QgsDualView::on_mFeatureList_currentEditSelectionChanged 0" << std::endl;
  if ( !mLayer->isEditable() || mAttributeForm->save() )
  {
    mAttributeForm->setFeature( feat );
    setCurrentEditSelection( QgsFeatureIds() << feat.id() );
  }
  else
  {
    // Couldn't save feature
  }
}

void QgsDualView::setCurrentEditSelection( const QgsFeatureIds &fids )
{
  std::cout << "QgsDualView::setCurrentEditSelection" << std::endl;
  mFeatureList->setCurrentFeatureEdited( false );
  mFeatureList->setEditSelection( fids );
}

bool QgsDualView::saveEditChanges()
{
  return mAttributeForm->save();
}

void QgsDualView::openConditionalStyles()
{
  mConditionalFormatWidget->setVisible( !mConditionalFormatWidget->isVisible() );
  mConditionalFormatWidget->viewRules();
}

void QgsDualView::setMultiEditEnabled( bool enabled )
{
  if ( enabled )
    setView( AttributeEditor );

  mAttributeForm->setMode( enabled ? QgsAttributeForm::MultiEditMode : QgsAttributeForm::SingleEditMode );
}

void QgsDualView::toggleSearchMode( bool enabled )
{
  if ( enabled )
  {
    setView( AttributeEditor );
    mAttributeForm->setMode( QgsAttributeForm::SearchMode );
  }
  else
  {
    mAttributeForm->setMode( QgsAttributeForm::SingleEditMode );
  }
}

void QgsDualView::previewExpressionBuilder()
{
  // Show expression builder
  QgsExpressionContext context( QgsExpressionContextUtils::globalProjectLayerScopes( mLayer ) );

  QgsExpressionBuilderDialog dlg( mLayer, mFeatureList->displayExpression(), this, QStringLiteral( "generic" ), context );
  dlg.setWindowTitle( tr( "Expression based preview" ) );
  dlg.setExpressionText( mFeatureList->displayExpression() );

  if ( dlg.exec() == QDialog::Accepted )
  {
    mFeatureList->setDisplayExpression( dlg.expressionText() );
    mFeatureListPreviewButton->setDefaultAction( mActionExpressionPreview );
    mFeatureListPreviewButton->setPopupMode( QToolButton::MenuButtonPopup );
  }

  mDisplayExpression = mFeatureList->displayExpression();
}

void QgsDualView::previewColumnChanged( QObject *action )
{
  QAction *previewAction = qobject_cast< QAction * >( action );

  if ( previewAction )
  {
    if ( !mFeatureList->setDisplayExpression( QStringLiteral( "COALESCE( \"%1\", '<NULL>' )" ).arg( previewAction->text() ) ) )
    {
      QMessageBox::warning( this,
                            tr( "Could not set preview column" ),
                            tr( "Could not set column '%1' as preview column.\nParser error:\n%2" )
                            .arg( previewAction->text(), mFeatureList->parserErrorString() )
                          );
    }
    else
    {
      mFeatureListPreviewButton->setDefaultAction( previewAction );
      mFeatureListPreviewButton->setPopupMode( QToolButton::InstantPopup );
    }
  }

  mDisplayExpression = mFeatureList->displayExpression();

  Q_ASSERT( previewAction );
}

int QgsDualView::featureCount()
{
  return mMasterModel->rowCount();
}

int QgsDualView::filteredFeatureCount()
{
  return mFilterModel->rowCount();
}

void QgsDualView::copyCellContent() const
{
  QAction *action = qobject_cast<QAction *>( sender() );

  if ( action && action->data().isValid() && action->data().canConvert<QModelIndex>() )
  {
    QModelIndex index = action->data().toModelIndex();
    QVariant var = masterModel()->data( index, Qt::DisplayRole );
    QApplication::clipboard()->setText( var.toString() );
  }
}

void QgsDualView::viewWillShowContextMenu( QMenu *menu, const QModelIndex &atIndex )
{
  if ( !menu )
  {
    return;
  }


  QModelIndex sourceIndex = mFilterModel->mapToSource( atIndex );

  QAction *copyContentAction = new QAction( tr( "Copy cell content" ), this );
  copyContentAction->setData( QVariant::fromValue<QModelIndex>( sourceIndex ) );
  menu->addAction( copyContentAction );
  connect( copyContentAction, &QAction::triggered, this, &QgsDualView::copyCellContent );

  QgsVectorLayer *vl = mFilterModel->layer();
  QgsMapCanvas *canvas = mFilterModel->mapCanvas();
  if ( canvas && vl && vl->geometryType() != QgsWkbTypes::NullGeometry )
  {
    menu->addAction( tr( "Zoom to feature" ), this, SLOT( zoomToCurrentFeature() ) );
    menu->addAction( tr( "Pan to feature" ), this, SLOT( panToCurrentFeature() ) );
  }

  //add user-defined actions to context menu
  QList<QgsAction> actions = mLayer->actions()->actions( QStringLiteral( "Field" ) );
  if ( !actions.isEmpty() )
  {
    QAction *a = menu->addAction( tr( "Run layer action" ) );
    a->setEnabled( false );

    Q_FOREACH ( const QgsAction &action, actions )
    {
      if ( !action.runable() )
        continue;

      QgsAttributeTableAction *a = new QgsAttributeTableAction( action.name(), this, action.id(), sourceIndex );
      menu->addAction( action.name(), a, SLOT( execute() ) );
    }
  }

  //add actions from QgsMapLayerActionRegistry to context menu
  QList<QgsMapLayerAction *> registeredActions = QgsMapLayerActionRegistry::instance()->mapLayerActions( mLayer );
  if ( !registeredActions.isEmpty() )
  {
    //add a separator between user defined and standard actions
    menu->addSeparator();

    QList<QgsMapLayerAction *>::iterator actionIt;
    for ( actionIt = registeredActions.begin(); actionIt != registeredActions.end(); ++actionIt )
    {
      QgsAttributeTableMapLayerAction *a = new QgsAttributeTableMapLayerAction( ( *actionIt )->text(), this, ( *actionIt ), sourceIndex );
      menu->addAction( ( *actionIt )->text(), a, SLOT( execute() ) );
    }
  }

  menu->addSeparator();
  QgsAttributeTableAction *a = new QgsAttributeTableAction( tr( "Open form" ), this, QString(), sourceIndex );
  menu->addAction( tr( "Open form" ), a, SLOT( featureForm() ) );
}

void QgsDualView::showViewHeaderMenu( QPoint point )
{
  int col = mTableView->columnAt( point.x() );

  delete mHorizontalHeaderMenu;
  mHorizontalHeaderMenu = new QMenu( this );

  QAction *hide = new QAction( tr( "&Hide column" ), mHorizontalHeaderMenu );
  connect( hide, &QAction::triggered, this, &QgsDualView::hideColumn );
  hide->setData( col );
  mHorizontalHeaderMenu->addAction( hide );
  QAction *setWidth = new QAction( tr( "&Set width..." ), mHorizontalHeaderMenu );
  connect( setWidth, &QAction::triggered, this, &QgsDualView::resizeColumn );
  setWidth->setData( col );
  mHorizontalHeaderMenu->addAction( setWidth );
  QAction *optimizeWidth = new QAction( tr( "&Autosize" ), mHorizontalHeaderMenu );
  connect( optimizeWidth, &QAction::triggered, this, &QgsDualView::autosizeColumn );
  optimizeWidth->setData( col );
  mHorizontalHeaderMenu->addAction( optimizeWidth );

  mHorizontalHeaderMenu->addSeparator();
  QAction *organize = new QAction( tr( "&Organize columns..." ), mHorizontalHeaderMenu );
  connect( organize, &QAction::triggered, this, &QgsDualView::organizeColumns );
  mHorizontalHeaderMenu->addAction( organize );
  QAction *sort = new QAction( tr( "&Sort..." ), mHorizontalHeaderMenu );
  connect( sort, &QAction::triggered, this, &QgsDualView::modifySort );
  mHorizontalHeaderMenu->addAction( sort );

  mHorizontalHeaderMenu->popup( mTableView->horizontalHeader()->mapToGlobal( point ) );
}

void QgsDualView::organizeColumns()
{
  if ( !mLayer )
  {
    return;
  }

  QgsOrganizeTableColumnsDialog dialog( mLayer, this );
  if ( dialog.exec() == QDialog::Accepted )
  {
    QgsAttributeTableConfig config = dialog.config();
    setAttributeTableConfig( config );
  }
}

void QgsDualView::tableColumnResized( int column, int width )
{
  QgsAttributeTableConfig config = mConfig;
  int sourceCol = config.mapVisibleColumnToIndex( column );
  if ( sourceCol >= 0 )
  {
    config.setColumnWidth( sourceCol, width );
    setAttributeTableConfig( config );
  }
}

void QgsDualView::hideColumn()
{
  QAction *action = qobject_cast<QAction *>( sender() );
  int col = action->data().toInt();
  QgsAttributeTableConfig config = mConfig;
  int sourceCol = mConfig.mapVisibleColumnToIndex( col );
  if ( sourceCol >= 0 )
  {
    config.setColumnHidden( sourceCol, true );
    setAttributeTableConfig( config );
  }
}

void QgsDualView::resizeColumn()
{
  QAction *action = qobject_cast<QAction *>( sender() );
  int col = action->data().toInt();
  if ( col < 0 )
    return;

  QgsAttributeTableConfig config = mConfig;
  int sourceCol = config.mapVisibleColumnToIndex( col );
  if ( sourceCol >= 0 )
  {
    bool ok = false;
    int width = QInputDialog::getInt( this, tr( "Set column width" ), tr( "Enter column width" ),
                                      mTableView->columnWidth( col ),
                                      0, 1000, 10, &ok );
    if ( ok )
    {
      config.setColumnWidth( sourceCol, width );
      setAttributeTableConfig( config );
    }
  }
}

void QgsDualView::autosizeColumn()
{
  QAction *action = qobject_cast<QAction *>( sender() );
  int col = action->data().toInt();
  mTableView->resizeColumnToContents( col );
}

void QgsDualView::modifySort()
{
  if ( !mLayer )
    return;

  QgsAttributeTableConfig config = mConfig;

  QDialog orderByDlg;
  orderByDlg.setWindowTitle( tr( "Configure attribute table sort order" ) );
  QDialogButtonBox *dialogButtonBox = new QDialogButtonBox( QDialogButtonBox::Ok | QDialogButtonBox::Cancel );
  QGridLayout *layout = new QGridLayout();
  connect( dialogButtonBox, &QDialogButtonBox::accepted, &orderByDlg, &QDialog::accept );
  connect( dialogButtonBox, &QDialogButtonBox::rejected, &orderByDlg, &QDialog::reject );
  orderByDlg.setLayout( layout );

  QGroupBox *sortingGroupBox = new QGroupBox();
  sortingGroupBox->setTitle( tr( "Defined sort order in attribute table" ) );
  sortingGroupBox->setCheckable( true );
  sortingGroupBox->setChecked( !sortExpression().isEmpty() );
  layout->addWidget( sortingGroupBox );
  sortingGroupBox->setLayout( new QGridLayout() );

  QgsExpressionBuilderWidget *expressionBuilder = new QgsExpressionBuilderWidget();
  QgsExpressionContext context( QgsExpressionContextUtils::globalProjectLayerScopes( mLayer ) );
  expressionBuilder->setExpressionContext( context );
  expressionBuilder->setLayer( mLayer );
  expressionBuilder->loadFieldNames();
  expressionBuilder->loadRecent( QStringLiteral( "generic" ) );
  expressionBuilder->setExpressionText( sortExpression().isEmpty() ? mLayer->displayExpression() : sortExpression() );

  sortingGroupBox->layout()->addWidget( expressionBuilder );

  QCheckBox *cbxSortAscending = new QCheckBox( tr( "Sort ascending" ) );
  cbxSortAscending->setChecked( config.sortOrder() == Qt::AscendingOrder );
  sortingGroupBox->layout()->addWidget( cbxSortAscending );

  layout->addWidget( dialogButtonBox );
  if ( orderByDlg.exec() )
  {
    Qt::SortOrder sortOrder = cbxSortAscending->isChecked() ? Qt::AscendingOrder : Qt::DescendingOrder;
    if ( sortingGroupBox->isChecked() )
    {
      setSortExpression( expressionBuilder->expressionText(), sortOrder );
      config.setSortExpression( expressionBuilder->expressionText() );
      config.setSortOrder( sortOrder );
    }
    else
    {
      setSortExpression( QString(), sortOrder );
      config.setSortExpression( QString() );
    }

    setAttributeTableConfig( config );
  }
}

void QgsDualView::zoomToCurrentFeature()
{
  QModelIndex currentIndex = mTableView->currentIndex();
  if ( !currentIndex.isValid() )
  {
    return;
  }

  QgsFeatureIds ids;
  ids.insert( mFilterModel->rowToId( currentIndex ) );
  QgsMapCanvas *canvas = mFilterModel->mapCanvas();
  if ( canvas )
  {
    canvas->zoomToFeatureIds( mLayer, ids );
  }
}

void QgsDualView::panToCurrentFeature()
{
  QModelIndex currentIndex = mTableView->currentIndex();
  if ( !currentIndex.isValid() )
  {
    return;
  }

  QgsFeatureIds ids;
  ids.insert( mFilterModel->rowToId( currentIndex ) );
  QgsMapCanvas *canvas = mFilterModel->mapCanvas();
  if ( canvas )
  {
    canvas->panToFeatureIds( mLayer, ids );
  }
}

void QgsDualView::rebuildFullLayerCache()
{
  connect( mLayerCache, &QgsVectorLayerCache::progress, this, &QgsDualView::progress, Qt::UniqueConnection );
  connect( mLayerCache, &QgsVectorLayerCache::finished, this, &QgsDualView::finished, Qt::UniqueConnection );

  mLayerCache->setFullCache( true );
}

void QgsDualView::previewExpressionChanged( const QString &expression )
{
  mLayer->setDisplayExpression( expression );
}

void QgsDualView::onSortColumnChanged()
{
  QgsAttributeTableConfig cfg = mLayer->attributeTableConfig();
  cfg.setSortExpression( mFilterModel->sortExpression() );
  cfg.setSortOrder( mFilterModel->sortOrder() );
  setAttributeTableConfig( cfg );
}

void QgsDualView::sortByPreviewExpression()
{
  Qt::SortOrder sortOrder = Qt::AscendingOrder;
  if ( mFeatureList->displayExpression() == sortExpression() )
  {
    sortOrder = mConfig.sortOrder() == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
  }
  setSortExpression( mFeatureList->displayExpression(), sortOrder );
}

void QgsDualView::updateSelectedFeatures()
{
  QgsFeatureRequest r = mMasterModel->request();
  if ( r.filterType() == QgsFeatureRequest::FilterNone && r.filterRect().isNull() )
    return; // already requested all features

  r.setFilterFids( masterModel()->layer()->selectedFeatureIds() );
  mMasterModel->setRequest( r );
  mMasterModel->loadLayer();
  emit filterChanged();
}

void QgsDualView::extentChanged()
{
  QgsFeatureRequest r = mMasterModel->request();
  if ( mMapCanvas && ( r.filterType() != QgsFeatureRequest::FilterNone || !r.filterRect().isNull() ) )
  {
    QgsRectangle rect = mMapCanvas->mapSettings().mapToLayerCoordinates( mLayer, mMapCanvas->extent() );
    r.setFilterRect( rect );
    mMasterModel->setRequest( r );
    mMasterModel->loadLayer();
  }
  emit filterChanged();
}

void QgsDualView::featureFormAttributeChanged()
{
  mFeatureList->setCurrentFeatureEdited( true );
}

void QgsDualView::setFilteredFeatures( const QgsFeatureIds &filteredFeatures )
{
  std::cout << "QgsDualView::setFilteredFeatures" << std::endl;
  mFilterModel->setFilteredFeatures( filteredFeatures );
}

void QgsDualView::setRequest( const QgsFeatureRequest &request )
{
  std::cout << "QgsDualView::setRequest" << std::endl;
  mMasterModel->setRequest( request );
}

void QgsDualView::setFeatureSelectionManager( QgsIFeatureSelectionManager *featureSelectionManager )
{
  mTableView->setFeatureSelectionManager( featureSelectionManager );
  mFeatureList->setFeatureSelectionManager( featureSelectionManager );

  if ( mFeatureSelectionManager && mFeatureSelectionManager->parent() == this )
    delete mFeatureSelectionManager;

  mFeatureSelectionManager = featureSelectionManager;
}

void QgsDualView::setAttributeTableConfig( const QgsAttributeTableConfig &config )
{
  mLayer->setAttributeTableConfig( config );
  mFilterModel->setAttributeTableConfig( config );
  mTableView->setAttributeTableConfig( config );
  mConfig = config;
}

void QgsDualView::setSortExpression( const QString &sortExpression, Qt::SortOrder sortOrder )
{
  if ( sortExpression.isNull() )
    mFilterModel->sort( -1 );
  else
    mFilterModel->sort( sortExpression, sortOrder );

  mConfig.setSortExpression( sortExpression );
  mConfig.setSortOrder( sortOrder );
  setAttributeTableConfig( mConfig );
}

QString QgsDualView::sortExpression() const
{
  return mFilterModel->sortExpression();
}

void QgsDualView::progress( int i, bool &cancel )
{
  if ( !mProgressDlg )
  {
    mProgressDlg = new QProgressDialog( tr( "Loading features..." ), tr( "Abort" ), 0, 0, this );
    mProgressDlg->setWindowTitle( tr( "Attribute table" ) );
    mProgressDlg->setWindowModality( Qt::WindowModal );
    mProgressDlg->show();
  }

  mProgressDlg->setLabelText( tr( "%1 features loaded." ).arg( i ) );
  QCoreApplication::processEvents();

  cancel = mProgressDlg && mProgressDlg->wasCanceled();
}

void QgsDualView::finished()
{
  delete mProgressDlg;
  mProgressDlg = nullptr;
}

/*
 * QgsAttributeTableAction
 */

void QgsAttributeTableAction::execute()
{
  std::cout << "QgsDualView::execute" << std::endl;
  mDualView->masterModel()->executeAction( mAction, mFieldIdx );
}

void QgsAttributeTableAction::featureForm()
{
  QgsFeatureIds editedIds;
  editedIds << mDualView->masterModel()->rowToId( mFieldIdx.row() );
  mDualView->setCurrentEditSelection( editedIds );
  mDualView->setView( QgsDualView::AttributeEditor );
}

/*
 * QgsAttributeTableMapLayerAction
 */

void QgsAttributeTableMapLayerAction::execute()
{
  mDualView->masterModel()->executeMapLayerAction( mAction, mFieldIdx );
}
