#include "ModuleBrowserPanel.h"

#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTabBar>
#include <QtWidgets/QLineEdit>

#include "editor/util.h"
#include "ModulePreviewList.h"
#include "editor/model/Library.h"

using namespace AxiomGui;

ModuleBrowserPanel::ModuleBrowserPanel(MainWindow *window, AxiomModel::Library *library, QWidget *parent)
    : DockPanel("Modules", parent), library(library) {
    setStyleSheet(AxiomUtil::loadStylesheet(":/ModuleBrowserPanel.qss"));

    auto mainLayout = new QGridLayout(this);
    auto mainWidget = new QWidget(this);
    mainWidget->setObjectName("mainWidget");

    mainLayout->setContentsMargins(0, 0, 0, 0);

    mainLayout->setColumnStretch(0, 10);
    mainLayout->setColumnStretch(1, 3);
    mainLayout->setColumnMinimumWidth(1, 200);
    mainLayout->setRowStretch(1, 1);

    filterTabs = new QTabBar(this);
    mainLayout->addWidget(filterTabs, 0, 0, Qt::AlignLeft);

    filterTabs->addTab(tr("All"));

    auto searchBox = new QLineEdit(this);
    searchBox->setObjectName("searchBox");
    searchBox->setPlaceholderText("Search modules...");
    mainLayout->addWidget(searchBox, 0, 1);

    auto previewList = new ModulePreviewList(window, library, this);
    mainLayout->addWidget(previewList, 1, 0, 1, 2);

    mainWidget->setLayout(mainLayout);
    setWidget(mainWidget);

    auto tags = library->tags();
    for (const auto &tag : tags) {
        addTag(tag);
    }
    connect(library, &AxiomModel::Library::tagAdded,
            this, &ModuleBrowserPanel::addTag);
    connect(library, &AxiomModel::Library::tagRemoved,
            this, &ModuleBrowserPanel::removeTag);
    connect(filterTabs, &QTabBar::currentChanged,
            this, &ModuleBrowserPanel::changeTag);
}

void ModuleBrowserPanel::addTag(const QString &tag) {
    auto index = filterTabs->addTab(tag);
    tabIndexes.emplace(tag, index);
    indexTabs.emplace(index, tag);
}

void ModuleBrowserPanel::removeTag(const QString &tag) {
    auto index = tabIndexes.find(tag);
    assert(index != tabIndexes.end());
    filterTabs->removeTab(index->second);
    tabIndexes.erase(index);
    indexTabs.erase(indexTabs.find(index->second));
}

void ModuleBrowserPanel::changeTag(int tag) {
    if (tag == 0) library->setActiveTag("");
    else {
        auto index = indexTabs.find(tag);
        assert(index != indexTabs.end());
        library->setActiveTag(index->second);
    }
}
