/*
  ***************************************************************************
  *   Copyright (C) 2011 by Phaneendra Hegde <phaneendra.hegde@gmail.com>   *
  *                                                                         *
  *   This program is free software; you can redistribute it and/or modify  *
  *   it under the terms of the GNU General Public License as published by  *
  *   the Free Software Foundation; either version 2 of the License, or     *
  *   (at your option) any later version.                                   *
  *                                                                         *
  *   This program is distributed in the hope that it will be useful,       *
  *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
  *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
  *   GNU General Public License for more details.                          *
  *                                                                         *
  *   You should have received a copy of the GNU General Public License     *
  *   along with this program; if not, write to the                         *
  *   Free Software Foundation, Inc.,                                       *
  *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA .        *
  ***************************************************************************
*/

//Local includes
#include "resourcebrowser.h"
#include "linkresourcedialog.h"
#include "removeduplicates.h"

//KDE Includes
#include <KXmlGuiWindow>
#include <KLocale>
#include <KIcon>
#include <KStatusBar>
#include <KAction>
#include <KRun>
#include <KDialog>
#include <KMenu>
#include <KPropertiesDialog>

//Qt includes
#include <QMessageBox>
#include <QListView>
#include <QDockWidget>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QDebug>
#include <QButtonGroup>
#include <QToolButton>
#include <QMenu>
#include <QLabel>
#include <QSize>
#include <QFile>
#include <QIODevice>

//Nepomuk Includes
#include <Nepomuk/ResourceManager>
#include <Nepomuk/Query/Term>
#include <Nepomuk/Query/Result>
#include <Nepomuk/Query/ResourceTypeTerm>
#include <Nepomuk/Query/ComparisonTerm>
#include <Nepomuk/Query/LiteralTerm>
#include <Nepomuk/Query/QueryServiceClient>
#include <Nepomuk/Vocabulary/PIMO>
#include <Nepomuk/Vocabulary/NCO>
#include <Nepomuk/Vocabulary/NFO>
#include <Nepomuk/Query/AndTerm>
#include <Nepomuk/Query/OrTerm>
#include <Nepomuk/Query/ResourceTerm>
#include <Nepomuk/Vocabulary/NIE>
#include <Nepomuk/Query/QueryParser>
#include <Nepomuk/Variant>
#include <Nepomuk/Tag>
#include <Nepomuk/Utils/FacetWidget>

//Soprano includes
#include <Soprano/QueryResultIterator>
#include <Soprano/Model>
#include <Soprano/Vocabulary/NAO>



ResourceBrowser::ResourceBrowser() :
    KXmlGuiWindow()
{
    Nepomuk::ResourceManager::instance()->init();
    setWindowTitle(i18n("Resource Browser"));
    setWindowIcon(KIcon("nepomuk"));

    buildCentralUI();
    setupDockWidgets();
    setupActions();
    setupModels();
    populateDefaultResources();
    setupGUI();
}


ResourceBrowser::~ResourceBrowser()
{
}


void ResourceBrowser::setupDockWidgets()
{
    QDockWidget* dock = new QDockWidget(i18n("Recommendation"),this);
    dock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_recommendationView = new QListView(dock);
    m_recommendationView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_recommendationView,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(slotRecommendedResourceContextMenu(QPoint)));
    dock->setWidget(m_recommendationView);
    addDockWidget(Qt::RightDockWidgetArea,dock);

    //TODO::if locked -> do this
    //dock->setFeatures(QDockWidget::NoDockWidgetFeatures);

    dock = new QDockWidget(i18n("Linked Resources"),this);
    dock->setAllowedAreas(Qt::RightDockWidgetArea);
    m_linkedResourceView = new QListView(dock);
    m_linkedResourceView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_linkedResourceView,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(slotLinkedResourceContextMenu(QPoint)));

    dock->setWidget(m_linkedResourceView);
    addDockWidget(Qt::RightDockWidgetArea,dock);

    dock = new QDockWidget("",this);
    dock->setAllowedAreas(Qt::LeftDockWidgetArea);
    QWidget* buttonWidget = new QWidget(dock);
    QVBoxLayout* buttonLayout = new QVBoxLayout(buttonWidget);
    m_manualLinkResourceButton = new QPushButton(buttonWidget);
    m_manualLinkResourceButton->setIcon(KIcon("insert-link"));
    m_manualLinkResourceButton->setText(i18n("Link resources manually"));
    m_manualLinkResourceButton->setEnabled(false);
    m_manualLinkResourceButton->setFlat(true);
    connect(m_manualLinkResourceButton,SIGNAL(clicked()),this,SLOT(slotManualLinkResources()));
    m_removeDuplicateButton = new QPushButton(buttonWidget);
    m_removeDuplicateButton->setIcon(KIcon("archive-remove"));
    m_removeDuplicateButton->setText(i18n("Remove Duplicates"));
    m_removeDuplicateButton->setFlat(true);
    connect(m_removeDuplicateButton, SIGNAL(clicked()), this, SLOT(slotRemoveDuplicates()));
    m_automaticTopicButton = new QPushButton( buttonWidget );
    m_automaticTopicButton->setIcon( KIcon("nepomuk") );
    m_automaticTopicButton->setText( i18n("Automatic Tagging") );
    m_automaticTopicButton->setEnabled( true );
    m_automaticTopicButton->setFlat( true );
    connect(m_automaticTopicButton, SIGNAL(clicked()), this, SLOT(slotAutomaticTopicSet()));
    buttonLayout->addWidget(m_manualLinkResourceButton);
    buttonLayout->addWidget(m_removeDuplicateButton);
    buttonLayout->addWidget( m_automaticTopicButton );
    dock->setWidget(buttonWidget);
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    addDockWidget(Qt::LeftDockWidgetArea,dock);

    dock = new QDockWidget(i18n("Resource Search "),this);
    Nepomuk::Utils::FacetWidget *searchWidget = new Nepomuk::Utils::FacetWidget(dock);
    searchWidget->addFacet(Nepomuk::Utils::Facet::createDateFacet(searchWidget));
    searchWidget->addFacet(Nepomuk::Utils::Facet::createTypeFacet(searchWidget));
    searchWidget->addFacet(Nepomuk::Utils::Facet::createRatingFacet(searchWidget));
    searchWidget->addFacet(Nepomuk::Utils::Facet::createPriorityFacet(searchWidget));
    searchWidget->addFacet(Nepomuk::Utils::Facet::createTagFacet(searchWidget));
    connect(searchWidget,SIGNAL(queryTermChanged(Nepomuk::Query::Term)),this,SLOT(slotFilterApplied(Nepomuk::Query::Term)));
    dock->setWidget(searchWidget);
    addDockWidget(Qt::LeftDockWidgetArea,dock);
    dock->setFeatures(QDockWidget::NoDockWidgetFeatures);
    connect (this, SIGNAL( sigShowProperties(KUrl) ), this, SLOT( slotShowProperties(KUrl)));

}


void ResourceBrowser::buildCentralUI()
{
    m_mainWidget = new QWidget(this);
    QVBoxLayout *gLayout = new QVBoxLayout(m_mainWidget);
    m_mainWidget->setLayout(gLayout);
    QHBoxLayout *hLayout = new QHBoxLayout(m_mainWidget);
    m_searchBox = new KLineEdit(m_mainWidget);
    m_searchBox->setClearButtonShown(true);
    m_searchBox->setPlaceholderText(i18n("Search for resources"));
    connect(m_searchBox,SIGNAL(textChanged(QString)),this,SLOT(slotTriggerSearch(QString)));
    m_resourceView = new QListView(m_mainWidget);
    m_resourceView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_resourceView->setViewMode(m_resourceView->IconMode);
    m_resourceView->setIconSize(QSize(42,42));
    m_resourceView->setUniformItemSizes(true);
    m_resourceSelect = new QComboBox(m_mainWidget);
    QStringList rList;
    rList << i18n("All Resources") << i18n("Movies") << i18n("Music") << i18n("Image") << i18n("Documents") << i18n("Contacts");
    m_resourceSelect->addItems( rList );
    hLayout->addWidget(m_resourceSelect);
    hLayout->addWidget(m_searchBox);
    gLayout->addLayout(hLayout);
    m_resourceView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_resourceView,SIGNAL(customContextMenuRequested(QPoint)),this,SLOT(slotResourceContextMenu(QPoint)));
    connect(m_resourceView,SIGNAL(doubleClicked(QModelIndex)),this,SLOT(slotOpenResource(QModelIndex)));
    QHBoxLayout* searchFilterLayout = new QHBoxLayout(m_mainWidget);
    m_resourceNameButton = new QToolButton(m_mainWidget);
    m_resourceNameButton->setText(i18n("Name"));
    m_resourceNameButton->setCheckable(true);
    m_resourceContentButton = new QToolButton(m_mainWidget);
    m_resourceContentButton->setText(i18n("Content"));
    m_resourceContentButton->setCheckable(true);
    m_resourceContentButton->setChecked(true);

    m_resourceTypeButton = new QToolButton(m_mainWidget);
    m_resourceTypeButton->setCheckable(true);
    m_resourceTypeButton->setText(i18n("Type"));

    searchFilterLayout->addWidget(m_resourceNameButton);
    searchFilterLayout->addWidget(m_resourceContentButton);
    searchFilterLayout->addWidget(m_resourceTypeButton);
    searchFilterLayout->setAlignment(Qt::AlignLeft);

    gLayout->addLayout(searchFilterLayout);
    gLayout->addWidget(m_resourceView);

    setCentralWidget(m_mainWidget);
}


void ResourceBrowser::setupActions()
{    
    m_unlinkAction = new KAction(this);
    m_unlinkAction->setText(i18n("&Unlink resource"));
    m_unlinkAction->setIcon(KIcon("edit-delete"));
    m_deleteAction = new KAction( this );
    m_deleteAction->setText( i18n(" &Delete Resource") );
    m_deleteAction->setIcon( KIcon("edit-delete"));
}


void ResourceBrowser::setupModels()
{
    m_resourceViewModel = new Nepomuk::Utils::SimpleResourceModel(this);
    m_resourceView->setModel(m_resourceViewModel);
    connect(m_resourceView->selectionModel(),SIGNAL(selectionChanged(QItemSelection,QItemSelection)),this,SLOT(slotLinkedResources()));
    connect(m_resourceView->selectionModel(),SIGNAL(selectionChanged(QItemSelection,QItemSelection)),this,SLOT(slotRecommendedResources()));
    m_recommendationViewModel = new Nepomuk::Utils::SimpleResourceModel(this);
    m_recommendationView->setModel(m_recommendationViewModel);
    connect(m_recommendationView,SIGNAL(doubleClicked(QModelIndex)),this,SLOT(slotOpenRecommendedResource(QModelIndex)));
    m_linkedResourceViewModel = new Nepomuk::Utils::SimpleResourceModel(this);
    m_linkedResourceView->setModel(m_linkedResourceViewModel);
    connect(m_linkedResourceView,SIGNAL(doubleClicked(QModelIndex)),this,SLOT(slotOpenLinkedResource(QModelIndex)));
}


void ResourceBrowser::slotOpenResource(QModelIndex selectedResource)
{
    m_resourceView->selectionModel()->setCurrentIndex(selectedResource,QItemSelectionModel::NoUpdate);
    Nepomuk::Resource currentResource = (m_resourceViewModel->resourceForIndex(
                                             m_resourceView->selectionModel()->currentIndex()));
    KUrl url = currentResource.property(Nepomuk::Vocabulary::NIE::url()).toString();
    qDebug()<<url;
    if(!url.isEmpty()) {
        new KRun(url,this);
        currentResource.increaseUsageCount();
    }
}


void ResourceBrowser::slotOpenRecommendedResource(QModelIndex selectedResource)
{
    m_recommendationView->selectionModel()->setCurrentIndex(selectedResource,QItemSelectionModel::NoUpdate);
    Nepomuk::Resource currentResource = (m_recommendationViewModel->resourceForIndex(
                                             m_recommendationView->selectionModel()->currentIndex()));
    KUrl url = currentResource.property(Nepomuk::Vocabulary::NIE::url()).toString();
    qDebug()<<url<<"usage count="<<currentResource.usageCount();
    if(!url.isEmpty()) {
        new KRun(url,this);
        currentResource.increaseUsageCount();
    }

}


void ResourceBrowser::slotOpenLinkedResource(QModelIndex selectedResource)
{
    m_linkedResourceView->selectionModel()->setCurrentIndex(selectedResource,QItemSelectionModel::NoUpdate);
    Nepomuk::Resource currentResource = (m_linkedResourceViewModel->resourceForIndex(
                                             m_linkedResourceView->selectionModel()->currentIndex()));
    KUrl url = currentResource.property(Nepomuk::Vocabulary::NIE::url()).toString();
    qDebug()<<url<<"usagecount="<<currentResource.usageCount();
    if(!url.isEmpty()) {
        new KRun(url,this);
        currentResource.increaseUsageCount();
    }
}


void ResourceBrowser::slotLinkedResources()
{
    m_linkedResourceViewModel->clear();
    m_recommendationViewModel->clear();
    m_manualLinkResourceButton->setEnabled(true);
    updateLinkedResources();
}


void ResourceBrowser::slotRecommendedResources()
{

    Nepomuk::Resource resource = m_resourceViewModel->resourceForIndex(m_resourceView->selectionModel()->currentIndex() );
    if(!resource.label().isEmpty() || !resource.genericLabel().isEmpty()) {
        QList<Nepomuk::Resource> recommendations;

        Q_FOREACH( const Nepomuk::Resource topic, resource.topics()) {
            qDebug() << topic;
            Nepomuk::Query::ComparisonTerm term( Nepomuk::Vocabulary::PIMO::Topic(),
                                                 Nepomuk::Query::ResourceTerm(topic) );
            Nepomuk::Query::Query query( term );
            QList<Nepomuk::Query::Result> results = Nepomuk::Query::QueryServiceClient::syncQuery( query );
            Q_FOREACH(Nepomuk::Query::Result rsc, results) {
                recommendations.append(rsc.resource());
            }
        }

        recommendations.append(contentResourceSearch(resource.genericLabel()));
        recommendations.append(contentResourceSearch(resource.label()));

        m_recommendationViewModel->setResources(recommendations);

    }
}


void ResourceBrowser::slotTriggerSearch( const QString str)
{
    m_resourceViewModel->clear();
    m_linkedResourceViewModel->clear();
    m_recommendationViewModel->clear();
    if(!str.isEmpty() && m_resourceContentButton->isChecked()) {
        m_resourceViewModel->clear();
        m_resourceViewModel->setResources(contentResourceSearch(str));
    }
    else if(!str.isEmpty() && m_resourceNameButton->isChecked()) {
        m_resourceViewModel->clear();
        m_resourceViewModel->setResources(nameResourceSearch(str));
    }
    else if(!str.isEmpty() && m_resourceTypeButton->isChecked()){
        m_resourceViewModel->clear();
        m_resourceViewModel->setResources(typeResourceSearch(str));
    }
    else {
        populateDefaultResources();
    }
}


void ResourceBrowser::slotFilterApplied(Nepomuk::Query::Term term)
{
    Nepomuk::Query::AndTerm query( m_currentQuery.term(),term );
    m_currentQuery.setTerm(query);

    m_currentQuery.setLimit( 25 );
    QList<Nepomuk::Query::Result> results = Nepomuk::Query::QueryServiceClient::syncQuery( m_currentQuery );
    QList<Nepomuk::Resource> resources;
    Q_FOREACH( const Nepomuk::Query::Result& result,results) {
        resources.append( result.resource() );
    }
    resourceSort(resources);
    m_resourceViewModel->setResources( resources );
}


void ResourceBrowser::slotResourceContextMenu(const QPoint &pos)
{
    m_propertyAction = new KAction(this);
    m_propertyAction->setText(i18n("&Properties "));
    m_propertyAction->setIcon(KIcon("documentinfo"));
    connect( m_propertyAction, SIGNAL(triggered()), this, SLOT(slotEmitResourceProperty()));
    connect( m_deleteAction, SIGNAL(triggered()), this, SLOT(slotDeleteResource()) );
    QMenu myMenu;
    QPoint globalPos = m_resourceView->mapToGlobal(pos);
    myMenu.addAction(m_propertyAction);
    myMenu.addAction(m_deleteAction);
    myMenu.exec(globalPos);
}


void ResourceBrowser::slotRecommendedResourceContextMenu(const QPoint &pos)
{
    m_propertyAction = new KAction(this);
    m_propertyAction->setText(i18n("&Properties "));
    m_propertyAction->setIcon(KIcon("documentinfo"));
    connect(m_propertyAction,SIGNAL(triggered()),this,SLOT(slotEmitRecommendedResourceProperty()));
    QMenu myMenu;
    QPoint globalPos = m_recommendationView->mapToGlobal(pos);
    myMenu.addAction(m_propertyAction);
    myMenu.exec(globalPos);
}


void ResourceBrowser::slotLinkedResourceContextMenu(const QPoint& pos )
{
    m_propertyAction = new KAction(this);
    m_propertyAction->setText(i18n("&Properties"));
    m_propertyAction->setIcon(KIcon("documentinfo"));

    connect(m_unlinkAction,SIGNAL(triggered(bool)),this,SLOT(slotUnlinkResource()));
    connect(m_propertyAction,SIGNAL(triggered(bool)),this,SLOT(slotEmitLinkedResourceProperty()));

    QMenu linkedResourceMenu;
    QPoint globalPos = m_linkedResourceView->mapToGlobal(pos);
    linkedResourceMenu.addAction(m_unlinkAction);
    linkedResourceMenu.addAction(m_propertyAction);
    linkedResourceMenu.exec(globalPos);
}


void ResourceBrowser:: slotEmitLinkedResourceProperty()
{
    KUrl url = (KUrl)m_linkedResourceViewModel->resourceForIndex(m_linkedResourceView->selectionModel()->currentIndex()).uri() ;
    qDebug()<<"url is "<<url;
    emit sigShowProperties(url);
}


void ResourceBrowser::slotEmitRecommendedResourceProperty()
{
    KUrl url = (KUrl)m_recommendationViewModel->resourceForIndex(m_recommendationView->selectionModel()->currentIndex()).uri() ;
    qDebug()<<"url is "<<url;
    emit sigShowProperties(url);
}


void ResourceBrowser::slotEmitResourceProperty()
{
    KUrl url = (KUrl)m_resourceViewModel->resourceForIndex(m_resourceView->selectionModel()->currentIndex()).uri() ;
    qDebug()<<"url is "<<url;
    emit sigShowProperties(url);
}


void ResourceBrowser::slotShowProperties(KUrl url)
{
    KPropertiesDialog *propertiesDialog = new KPropertiesDialog(url,this);
    propertiesDialog->exec();
}


void ResourceBrowser::slotManualLinkResources()
{
    LinkResourceDialog manualLinkDialog(m_resourceViewModel->resourceForIndex(m_resourceView->currentIndex()));
    manualLinkDialog.exec();
    updateLinkedResources();
}


void ResourceBrowser::slotUnlinkResource()
{
    m_resourceViewModel->resourceForIndex(m_resourceView->selectionModel()->currentIndex() ).removeProperty(
                Nepomuk::Resource::isRelatedUri(),m_linkedResourceViewModel->resourceForIndex(
                    m_linkedResourceView->selectionModel()->currentIndex()));
    updateLinkedResources();
}


void ResourceBrowser::slotRemoveDuplicates()
{
    RemoveDuplicates *duplicates = new RemoveDuplicates();
    duplicates->exec();
}

void ResourceBrowser::slotDeleteResource()
{
    QMessageBox msgBox;
    msgBox.setText("Alert");
    msgBox.setInformativeText("Do you really want to delete this resource? ");
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No );
    msgBox.setDefaultButton(QMessageBox::No);
    msgBox.setIcon(QMessageBox::Warning);
    int ret = msgBox.exec();
    if(ret == QMessageBox::Yes) {
        qDebug()<<m_resourceViewModel->resourceForIndex(m_resourceView->selectionModel()->currentIndex()).label();
        m_resourceViewModel->resourceForIndex(m_resourceView->selectionModel()->currentIndex()).remove();
        //populateDefaultResources();
        //TODO:: Populate pervious resource list, Not the default one
        populatePreviousResourceList();
    }
}

void ResourceBrowser::slotAutomaticTopicSet()
{
    QFile fileSW("/home/nokia-pg/kde/src/resourcebrowser/stopwords.txt");
    QFile fileDict("/home/nokia-pg/kde/src/resourcebrowser/corncob_lowercase.txt");

    if (!fileSW.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug()<<"Can't open stopword file";
    }


    if (!fileDict.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug()<<"Can't open dictionary";
    }

    QStringList list,stopwords,dictionary;
    QHash<QString,int>hash;

    QTextStream sw(&fileSW);
    while(!sw.atEnd()) {
        QString line = sw.readLine();
        stopwords += line.split(" ");
    }

    QTextStream dict(&fileDict);
    while(!dict.atEnd()) {
        QString line = dict.readLine();
        dictionary += line.split(" ");
    }

    Nepomuk::Query::Query query;
    query.setLimit(10);
    Nepomuk::Query::Term term;
    term = Nepomuk::Query::ResourceTypeTerm(Nepomuk::Vocabulary::NFO::TextDocument()) ||
            Nepomuk::Query::ComparisonTerm(Nepomuk::Vocabulary::NIE::mimeType(),
                                           Nepomuk::Query::LiteralTerm(QLatin1String("text/plain")));
    query.setTerm(term);
    QList<Nepomuk::Query::Result>results = Nepomuk::Query::QueryServiceClient::syncQuery( query );



    Q_FOREACH( const Nepomuk::Query::Result& result, results ) {
        if(result.resource().topics().length() <=3 ) {
            Nepomuk::Variant v = result.resource().property(Nepomuk::Vocabulary::NIE::plainTextContent());

            QString line = v.toString();
            list = line.split(" ");
            qDebug()<<"asdf"<<result.resource();
            foreach(QString str,list) {
                // add a check to see if word is present in dictionary, if not ignore!


                if(!stopwords.contains(str)) {
                    if(hash.value(str)) {
                        hash[str] = hash[str]+1;
                        qDebug()<<"in if";
                    }
                    else
                        hash[str] = 1;
                }

            }

            QHashIterator<QString, int> l(hash);
            while (l.hasNext()) {
                l.next();
            }

            QList<int> vals = hash.values();
            QSet<int>set = vals.toSet();
            vals= set.toList();
            qSort(vals);
            QStringList string;
            while(string.length()<5) {
                string = string+hash.keys(vals.last());
                vals.removeLast();
            }
            qDebug()<<string << "File name:->>>>" << result.resource().genericLabel();

            for(int i=0;i<3;i++) {
                Nepomuk::Resource topic(string[i]);
                topic.addType(Nepomuk::Vocabulary::PIMO::Topic());
                result.resource().addTopic(topic);
            }
        }
    }
}


void ResourceBrowser::populateDefaultResources()
{
    m_resourceViewModel->clear();
    Nepomuk::Query::Term term =  Nepomuk::Query::ResourceTypeTerm( Nepomuk::Vocabulary::NFO::Website() );
    m_currentQuery.setTerm(term);
    m_currentQuery = m_currentQuery || Nepomuk::Query::ResourceTypeTerm(Nepomuk::Vocabulary::PIMO::Person() );
    m_currentQuery = m_currentQuery || Nepomuk::Query::ResourceTypeTerm(Nepomuk::Vocabulary::NFO::PaginatedTextDocument() );
    m_currentQuery.setLimit( 35 );
    QList<Nepomuk::Query::Result> results = Nepomuk::Query::QueryServiceClient::syncQuery( m_currentQuery );
    QList<Nepomuk::Resource> resources;
    Q_FOREACH( const Nepomuk::Query::Result& result,results) {
        addIconToResource(result.resource());
        qDebug()<<result.resource().genericIcon();
        resources.append( result.resource() );
    }
    resourceSort(resources);
    m_resourceViewModel->setResources( resources );
}


void ResourceBrowser::addIconToResource(Nepomuk::Resource rsc)
{

    if(rsc.className().compare("Folder") == 0) {
        rsc.addSymbol("folder-blue");
    }
    else if(rsc.className().compare("RasterImage") == 0) {
        rsc.addSymbol("image-x-generic");
    }
    else if(rsc.className().compare("Document") == 0) {
        rsc.addSymbol("libreoffice34-oasis-master-document");
    }
    else if(rsc.className().compare("Audio") == 0) {
        rsc.addSymbol("audio-basic");
    }
    else if(rsc.className().compare("Video") == 0) {
        rsc.addSymbol("video-x-generic");
    }
    else if(rsc.className().compare("InformationElement") == 0) {
        rsc.addSymbol("video-x-generic");
    }
    else if(rsc.className().compare("TextDocument") == 0) {
        rsc.addSymbol("text-x-generic");
    }
    else if(rsc.className().compare("PaginatedTextDocument") == 0) {
        rsc.addSymbol("application-pdf");
    }
    else if(rsc.className().compare("Archive") == 0) {
        rsc.addSymbol("application-x-compressed");
    }
    else if(rsc.className().compare("Person") == 0){
        rsc.addSymbol("user-identity");
    }
    else if(rsc.className().compare("Website") == 0) {
        rsc.addSymbol("text-html");
    }

}


void ResourceBrowser::resourceSort(QList<Nepomuk::Resource> &resources)
{
    for (int i=0; i<resources.size()-1; i++) {
        for (int j=0; j<resources.size()-1; j++) {
            if (resources.at(j).usageCount() < resources.at(j+1).usageCount()) {
                Nepomuk::Resource temp = resources.at(j);
                resources.replace(j,resources.at(j+1));
                resources.replace(j+1,temp);
            }
        }
    }
}


QList<Nepomuk::Resource> ResourceBrowser::contentResourceSearch(const QString str)
{
    Nepomuk::Query::ComparisonTerm linkTerm(Nepomuk::Vocabulary::NIE::plainTextContent(), Nepomuk::Query::LiteralTerm(str));
    m_currentQuery.setTerm(linkTerm);
    m_currentQuery.setLimit(25);
    QList<Nepomuk::Query::Result>results = Nepomuk::Query::QueryServiceClient::syncQuery( m_currentQuery );
    QList<Nepomuk::Resource> resource;
    Q_FOREACH( const Nepomuk::Query::Result& result, results ) {
        resource.append( result.resource() );
    }
    resourceSort(resource);
    return resource;
}


QList<Nepomuk::Resource> ResourceBrowser::nameResourceSearch(const QString str)
{
    QString regex = QRegExp::escape(str);
    regex.replace("\\*", QLatin1String(".*"));
    regex.replace("\\?", QLatin1String("."));
    regex.replace("\\", "\\\\");
    Nepomuk::Query::ComparisonTerm linkTerm(
                Nepomuk::Vocabulary::NFO::fileName(),
                Nepomuk::Query::LiteralTerm(regex),
                Nepomuk::Query::ComparisonTerm::Regexp);
    Nepomuk::Query::ComparisonTerm temp(Soprano::Vocabulary::NAO::prefLabel(),Nepomuk::Query::LiteralTerm(str));
    Nepomuk::Query::OrTerm query( linkTerm,temp );
    m_currentQuery.setTerm(query);
    m_currentQuery.setLimit(50);

    QList<Nepomuk::Query::Result>results = Nepomuk::Query::QueryServiceClient::syncQuery( m_currentQuery );
    QList<Nepomuk::Resource> resource;
    Q_FOREACH( const Nepomuk::Query::Result& result, results ) {
        addIconToResource(result.resource());
        resource.append( result.resource() );
    }
    resourceSort(resource);
    return resource;
}


QList<Nepomuk::Resource> ResourceBrowser:: typeResourceSearch(const QString str)
{
    Nepomuk::Query::Term linkTerm;
    //m_currentQuery.setLimit(50);
    if(str.contains("music") || str.contains("songs") || str.contains("audio")) {
        linkTerm =  Nepomuk::Query::ResourceTypeTerm(Nepomuk::Vocabulary::NFO::Audio()) ||
                Nepomuk::Query::ComparisonTerm(Nepomuk::Vocabulary::NIE::mimeType(),
                                               Nepomuk::Query::LiteralTerm(QLatin1String("audio")));
    }
    else if(str.contains("video") || str.contains("movie")) {
        linkTerm =  Nepomuk::Query::ResourceTypeTerm(Nepomuk::Vocabulary::NFO::Video()) ||
                Nepomuk::Query::ComparisonTerm(Nepomuk::Vocabulary::NIE::mimeType(),
                                               Nepomuk::Query::LiteralTerm(QLatin1String("video")));
    }
    else if(str.contains("photo") || str.contains("picture") || str.contains("image")) {
        linkTerm =  Nepomuk::Query::ResourceTypeTerm( Nepomuk::Vocabulary::NFO::Image() );
    }
    else if(str.contains("archive") || str.contains("compressed") ) {
        linkTerm =  Nepomuk::Query::ResourceTypeTerm( Nepomuk::Vocabulary::NFO::Archive() );
    }
    else if(str.contains("pdf")) {
        linkTerm =  Nepomuk::Query::ResourceTypeTerm( Nepomuk::Vocabulary::NFO::PaginatedTextDocument() );
    }
    else if(str.contains("ppt") || str.contains("presentation")) {
        linkTerm =  Nepomuk::Query::ResourceTypeTerm( Nepomuk::Vocabulary::NFO::Presentation() );
    }
    else if(str.contains("text") || str.contains("txt") || str.contains("document") || str.contains("doc")  ) {
        linkTerm =  Nepomuk::Query::ResourceTypeTerm( Nepomuk::Vocabulary::NFO::Document() );
    }
    else if(str.contains("contact") || str.contains("person") ) {
        m_currentQuery.setLimit(200);
        linkTerm =  Nepomuk::Query::ResourceTypeTerm( Nepomuk::Vocabulary::NCO::PersonContact() );
    }
    else if(str.contains("java") ) {
        qDebug()<<"Found";
        QString regex = ".java$";
        regex.replace("\\*", QLatin1String(".*"));
        regex.replace("\\?", QLatin1String("."));
        regex.replace("\\", "\\\\");
        linkTerm =  Nepomuk::Query::ComparisonTerm( Nepomuk::Vocabulary::NFO::TextDocument(),Nepomuk::Query::LiteralTerm(regex),
                                                    Nepomuk::Query::ComparisonTerm::Regexp);

    }
    m_currentQuery.setTerm(linkTerm);
    QList<Nepomuk::Query::Result>results = Nepomuk::Query::QueryServiceClient::syncQuery( m_currentQuery );
    QList<Nepomuk::Resource> resource;
    if(results.length() > 30 ) {
        m_resourceView->setViewMode(m_resourceView->ListMode);
    }
    else {
        m_resourceView->setViewMode(m_resourceView->IconMode);
    }
    Q_FOREACH( const Nepomuk::Query::Result& result, results ) {
        addIconToResource(result.resource());
        resource.append( result.resource() );
    }
    resourceSort(resource);
    return resource;
}
void ResourceBrowser::updateLinkedResources()
{
    Nepomuk::Resource resource = m_resourceViewModel->resourceForIndex(m_resourceView->selectionModel()->currentIndex() );
    QList<Nepomuk::Resource> relatedResourceList = resource.isRelatedOf();
    relatedResourceList.append(resource.isRelateds());
    if(! relatedResourceList.isEmpty() ) {
        resourceSort(relatedResourceList);
        m_linkedResourceViewModel->setResources(relatedResourceList);
    }
}

void ResourceBrowser::populatePreviousResourceList()
{
    if(! m_currentQuery.isValid()) {
        populateDefaultResources();
    }
    QList<Nepomuk::Query::Result>results = Nepomuk::Query::QueryServiceClient::syncQuery( m_currentQuery );
    QList<Nepomuk::Resource> resource;
    Q_FOREACH( const Nepomuk::Query::Result& result, results ) {
        addIconToResource( result.resource() );
        resource.append( result.resource() );
    }
    resourceSort(resource);
    m_resourceViewModel->setResources(resource);
}

#include "resourcebrowser.moc"
