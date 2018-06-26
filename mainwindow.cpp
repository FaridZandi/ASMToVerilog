#include "arrow.h"
#include "diagramitem.h"
#include "diagramscene.h"
#include "diagramtextitem.h"
#include "mainwindow.h"
#include "asmblock.h"

#include <QtWidgets>
#include <fstream>
#include <map>
#include <sstream>

const int InsertTextButton = 10;

MainWindow::MainWindow()
{
    createActions();
    createToolBox();
    createMenus();

    scene = new DiagramScene(itemMenu, this);
    scene->setSceneRect(QRectF(0, 0, 5000, 5000));
    connect(scene, SIGNAL(itemInserted(DiagramItem*)),
            this, SLOT(itemInserted(DiagramItem*)));
    connect(scene, SIGNAL(textInserted(QGraphicsTextItem*)),
            this, SLOT(textInserted(QGraphicsTextItem*)));
    connect(scene, SIGNAL(itemSelected(QGraphicsItem*)),
            this, SLOT(itemSelected(QGraphicsItem*)));
    createToolbars();

    QHBoxLayout *layout = new QHBoxLayout;
    layout->addWidget(toolBox);
    view = new QGraphicsView(scene);
    layout->addWidget(view);

    QWidget *widget = new QWidget;
    widget->setLayout(layout);

    setCentralWidget(widget);
    setWindowTitle(tr("Diagramscene"));
    setUnifiedTitleAndToolBarOnMac(true);
}

void MainWindow::backgroundButtonGroupClicked(QAbstractButton *button)
{
    QList<QAbstractButton *> buttons = backgroundButtonGroup->buttons();
    foreach (QAbstractButton *myButton, buttons) {
        if (myButton != button)
            button->setChecked(false);
    }
    QString text = button->text();
    if (text == tr("Blue Grid"))
        scene->setBackgroundBrush(QPixmap(":/images/background1.png"));
    else if (text == tr("White Grid"))
        scene->setBackgroundBrush(QPixmap(":/images/background2.png"));
    else if (text == tr("Gray Grid"))
        scene->setBackgroundBrush(QPixmap(":/images/background3.png"));
    else
        scene->setBackgroundBrush(QPixmap(":/images/background4.png"));

    scene->update();
    view->update();
}

void MainWindow::buttonGroupClicked(int id)
{
    QList<QAbstractButton *> buttons = buttonGroup->buttons();
    foreach (QAbstractButton *button, buttons) {
        if (buttonGroup->button(id) != button)
            button->setChecked(false);
    }
    if (id == InsertTextButton) {
        scene->setMode(DiagramScene::InsertText);
    } else {
        scene->setItemType(DiagramItem::DiagramType(id));
        scene->setMode(DiagramScene::InsertItem);
    }
}

void MainWindow::deleteItem()
{
    foreach (QGraphicsItem *item, scene->selectedItems()) {
        if (item->type() == Arrow::Type) {
            scene->removeItem(item);
            Arrow *arrow = qgraphicsitem_cast<Arrow *>(item);
            arrow->startItem()->removeArrow(arrow);
            arrow->endItem()->removeArrow(arrow);
            delete item;
        }
    }

    foreach (QGraphicsItem *item, scene->selectedItems()) {
         if (item->type() == DiagramItem::Type)
             qgraphicsitem_cast<DiagramItem *>(item)->removeArrows();
         scene->removeItem(item);
         delete item;
    }
}

void MainWindow::enlargeShape(){
    foreach (QGraphicsItem *item, scene->selectedItems()) {
        if (item->type() == DiagramItem::Type) {
            qgraphicsitem_cast<DiagramItem *>(item)->enlarge();
        }
    }
}

void MainWindow::shrinkShape(){
    foreach (QGraphicsItem *item, scene->selectedItems()) {
        if (item->type() == DiagramItem::Type) {
            qgraphicsitem_cast<DiagramItem *>(item)->shrink();
        }
    }
}

DiagramItem* findArrowDest(Arrow* a, std::string& arrow_text, Arrow* &last_arrow_piece){
    DiagramItem* current = a->endItem();
    Arrow* currentArrow = a;
    if(currentArrow->textItem){
        arrow_text = currentArrow->textItem->toPlainText().toStdString();
    }
    last_arrow_piece = a;

    while(current->diagramType() == DiagramItem::DiagramType::Point){

        if(currentArrow->textItem){
            arrow_text = currentArrow->textItem->toPlainText().toStdString();
        }

        Arrow* nextArrow;

        for(auto arrow : current->getArrows()){
            if(arrow != currentArrow){
                nextArrow = arrow;
                break;
            }
        }
        if(not nextArrow){
            return NULL;
        }

        current = nextArrow->endItem();
        currentArrow = nextArrow;
        last_arrow_piece = currentArrow;
    }
    return current;
}


void traverse(bool is_root, DiagramItem* item, Arrow* current_arrow, std::vector<condition> conditions, ASMBlock& asm_block,
              std::map<DiagramItem*, int>& states){

    std::cout << item << std::endl;

    std::string text;

    if(item->textItem){
        text = item->textItem->toPlainText().toStdString();
    } else {
        text = "";
    }

    switch(item->diagramType()){
    case DiagramItem::DiagramType::Step:
        if(not is_root){
            next_state next{conditions, states[item]};
            asm_block.next_states.push_back(next);
            return;
        } else {
            asm_block.default_code = text;
        }
        break;
    case DiagramItem::DiagramType::Conditional:
        break;
    case DiagramItem::DiagramType::Io:{
        conditional_code code{conditions, text};
        asm_block.conditional_codes.push_back(code);
        break;
    }

    default:
        std::cout << "don't know what to do" << std::endl;
        break;
    }

    auto arrows = item->getArrows();

    std::cout << arrows.size() << std::endl;

    for(auto arrow : arrows){
        if(arrow->startItem() == item){
            std::string arrow_value;

            Arrow* last_arrow_piece;

            DiagramItem* arrowDest = findArrowDest(arrow, arrow_value, last_arrow_piece);

            if(item->diagramType() == DiagramItem::DiagramType::Conditional){
                condition c{text, arrow_value};
                auto new_conditions = conditions;
                new_conditions.push_back(c);

                traverse(false, arrowDest, last_arrow_piece, new_conditions, asm_block, states);
            } else {
                traverse(false, arrowDest, last_arrow_piece, conditions, asm_block, states);
            }
        }
    }
}

void MainWindow::convert(){
    string fileName = "mori.v";

    std::map<DiagramItem*, int> states;

    int counter = 0;
    for(auto i : scene->items()){
        if(i->type() == DiagramItem::Type){
            DiagramItem *diagramItem = qgraphicsitem_cast<DiagramItem *>(i);
            if(diagramItem->diagramType() == DiagramItem::DiagramType::Step){
                states.insert(std::make_pair(diagramItem, counter));
                counter ++;
            }
        }
    }

    std::vector<ASMBlock> asm_blocks;

    for(auto state = states.begin(); state != states.end(); state++){
        ASMBlock asm_block(state->second);

        traverse(true, state->first, NULL, std::vector<condition>(), asm_block, states);

        asm_blocks.push_back(asm_block);
    }
    cout << fileName << endl;
    ofstream verilogCode;

    //combo. segment
    /////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////
    cout << "always @ (posedge clock)" << endl;
    cout << "begin" << endl;
    cout << "if (reset == 1'b1) begin" << endl;
    cout << "\t state <= 0;" << endl;

    cout << "end else" << endl;
    cout << "case (state)" << endl;
    for(auto asmBlock : asm_blocks)
    {
        cout <<  asmBlock.id << ":" << "begin" << endl;

        stringstream ss(asmBlock.default_code);
        string rt;

        if(asmBlock.default_code != "")
        {
            while(getline(ss , rt , '\n'))
                cout << "\t" << rt << ";" << endl;
        }
        bool flag = true;
        for(auto conditionBox : asmBlock.conditional_codes)
        {
            if(flag)
                cout << "\tif (";
            else cout << "\telse if(";
            flag = false;
            int condSize = conditionBox.conditions.size();
            if(condSize == 1)
            {
                if(atoi(conditionBox.conditions[0].value.c_str()) == 1)
                    cout <<  conditionBox.conditions[0].condition ;
                else
                    cout << "(!(" << conditionBox.conditions[0].condition << "))";
            }
            else
            for(int i = 0 ; i < condSize; i++)
            {
                if(atoi(conditionBox.conditions[i].value.c_str()) == 1)
                    cout << "(" << conditionBox.conditions[i].condition << ")";
                else
                    cout << "(!( " << conditionBox.conditions[i].condition << "))";
                if (i < condSize - 1)
                    cout << " && " ;
            }
            cout << ") begin"<< endl;

            stringstream ss(conditionBox.code);
            string rt;

            if(conditionBox.code != "")
            {
                while(getline(ss , rt , '\n'))
                    cout << "\t\t" << rt << ";" << endl;
            }
            cout << "\tend" << endl;
        }

    }
    cout << "end" << endl;
    cout << "endcase" << endl;
    //seq. segment
  ///////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////

    //nextState generator

    cout << "always @ (posedge clock)" << endl;
    cout << "begin" << endl;
    cout << "if (reset == 1'b1) begin" << endl;
    cout << "\t state <= 0;" << endl;

    cout << "end else" << endl;
    cout << "case (state)" << endl;
    for(auto asmBlock : asm_blocks)
    {
        cout <<  asmBlock.id << ":" << "begin" << endl;
        bool flag = true;
        for(auto stateBox : asmBlock.next_states)
        {
            if(flag)
            cout << "\tif (";
            else cout << "\telse if (";
            flag = false;
            int stateSize = stateBox.conditions.size();
            if(stateSize == 1)
            {
                if(atoi(stateBox.conditions[0].value.c_str()) == 1)
                    cout <<  stateBox.conditions[0].condition ;
                else
                    cout << "(!(" << stateBox.conditions[0].condition << "))";
            }
            else
            for(int i = 0 ; i < stateSize ; i++)
            {
                if(atoi(stateBox.conditions[i].value.c_str()) == 1)
                    cout << "(" << stateBox.conditions[i].condition << ")";
                else
                    cout << "(!(" << stateBox.conditions[i].condition << "))";
                if (i < stateBox.conditions.size() - 1)
                    cout << " && " ;
            }
            cout << ")"<< endl;
            cout << "\t\tstate <= " << stateBox.next_state_id << ";" << endl;
        }
    cout << "end" << endl;
    }
    cout << "end" << endl;
    cout << "endcase" << endl;



}

void MainWindow::pointerGroupClicked(int)
{
    scene->setMode(DiagramScene::Mode(pointerTypeGroup->checkedId()));
}

void MainWindow::bringToFront()
{
    if (scene->selectedItems().isEmpty())
        return;

    QGraphicsItem *selectedItem = scene->selectedItems().first();
    QList<QGraphicsItem *> overlapItems = selectedItem->collidingItems();

    qreal zValue = 0;
    foreach (QGraphicsItem *item, overlapItems) {
        if (item->zValue() >= zValue && item->type() == DiagramItem::Type)
            zValue = item->zValue() + 0.1;
    }
    selectedItem->setZValue(zValue);
}

void MainWindow::sendToBack()
{
    if (scene->selectedItems().isEmpty())
        return;

    QGraphicsItem *selectedItem = scene->selectedItems().first();
    QList<QGraphicsItem *> overlapItems = selectedItem->collidingItems();

    qreal zValue = 0;
    foreach (QGraphicsItem *item, overlapItems) {
        if (item->zValue() <= zValue && item->type() == DiagramItem::Type)
            zValue = item->zValue() - 0.1;
    }
    selectedItem->setZValue(zValue);
}

void MainWindow::itemInserted(DiagramItem *item)
{
    pointerTypeGroup->button(int(DiagramScene::MoveItem))->setChecked(true);
    scene->setMode(DiagramScene::Mode(pointerTypeGroup->checkedId()));
    buttonGroup->button(int(item->diagramType()))->setChecked(false);
}

void MainWindow::textInserted(QGraphicsTextItem *)
{
    buttonGroup->button(InsertTextButton)->setChecked(false);
    scene->setMode(DiagramScene::Mode(pointerTypeGroup->checkedId()));
}

void MainWindow::currentFontChanged(const QFont &)
{
    handleFontChange();
}

void MainWindow::fontSizeChanged(const QString &)
{
    handleFontChange();
}

void MainWindow::sceneScaleChanged(const QString &scale)
{
    double newScale = scale.left(scale.indexOf(tr("%"))).toDouble() / 100.0;
    QMatrix oldMatrix = view->matrix();
    view->resetMatrix();
    view->translate(oldMatrix.dx(), oldMatrix.dy());
    view->scale(newScale, newScale);
}

void MainWindow::textColorChanged()
{
    textAction = qobject_cast<QAction *>(sender());
    fontColorToolButton->setIcon(createColorToolButtonIcon(
                                     ":/images/textpointer.png",
                                     qvariant_cast<QColor>(textAction->data())));
    textButtonTriggered();
}

void MainWindow::itemColorChanged()
{
    fillAction = qobject_cast<QAction *>(sender());
    fillColorToolButton->setIcon(createColorToolButtonIcon(
                                     ":/images/floodfill.png",
                                     qvariant_cast<QColor>(fillAction->data())));
    fillButtonTriggered();
}

void MainWindow::lineColorChanged()
{
    lineAction = qobject_cast<QAction *>(sender());
    lineColorToolButton->setIcon(createColorToolButtonIcon(
                                     ":/images/linecolor.png",
                                     qvariant_cast<QColor>(lineAction->data())));
    lineButtonTriggered();
}

void MainWindow::textButtonTriggered()
{
    scene->setTextColor(qvariant_cast<QColor>(textAction->data()));
}

void MainWindow::fillButtonTriggered()
{
    scene->setItemColor(qvariant_cast<QColor>(fillAction->data()));
}

void MainWindow::lineButtonTriggered()
{
    scene->setLineColor(qvariant_cast<QColor>(lineAction->data()));
}

void MainWindow::handleFontChange()
{
    QFont font = fontCombo->currentFont();
    font.setPointSize(fontSizeCombo->currentText().toInt());
    font.setWeight(boldAction->isChecked() ? QFont::Bold : QFont::Normal);
    font.setItalic(italicAction->isChecked());
    font.setUnderline(underlineAction->isChecked());

    scene->setFont(font);
}

void MainWindow::itemSelected(QGraphicsItem *item)
{
    DiagramTextItem *textItem =
    qgraphicsitem_cast<DiagramTextItem *>(item);

    QFont font = textItem->font();
    fontCombo->setCurrentFont(font);
    fontSizeCombo->setEditText(QString().setNum(font.pointSize()));
    boldAction->setChecked(font.weight() == QFont::Bold);
    italicAction->setChecked(font.italic());
    underlineAction->setChecked(font.underline());
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About Diagram Scene"),
                       tr("The <b>Diagram Scene</b> example shows "
                          "use of the graphics framework."));
}

void MainWindow::createToolBox()
{
    buttonGroup = new QButtonGroup(this);
    buttonGroup->setExclusive(false);
    connect(buttonGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(buttonGroupClicked(int)));
    QGridLayout *layout = new QGridLayout;
    layout->addWidget(createCellWidget(tr("Decision Box"), DiagramItem::Conditional), 0, 0);
    layout->addWidget(createCellWidget(tr("State Box"), DiagramItem::Step),0, 1);
    layout->addWidget(createCellWidget(tr("Conditional Box"), DiagramItem::Io), 1, 0);

    QToolButton *textButton = new QToolButton;
    textButton->setCheckable(true);
    buttonGroup->addButton(textButton, InsertTextButton);
    textButton->setIcon(QIcon(QPixmap(":/images/comment.png")));
    textButton->setIconSize(QSize(50, 50));
    QGridLayout *textLayout = new QGridLayout;
    textLayout->addWidget(textButton, 0, 0, Qt::AlignHCenter);
    textLayout->addWidget(new QLabel(tr("Comment")), 1, 0, Qt::AlignCenter);
    QWidget *textWidget = new QWidget;
    textWidget->setLayout(textLayout);
    layout->addWidget(textWidget, 1, 1);

    layout->setRowStretch(3, 10);
    layout->setColumnStretch(2, 10);

    QWidget *itemWidget = new QWidget;
    itemWidget->setLayout(layout);

    backgroundButtonGroup = new QButtonGroup(this);
    connect(backgroundButtonGroup, SIGNAL(buttonClicked(QAbstractButton*)),
            this, SLOT(backgroundButtonGroupClicked(QAbstractButton*)));

    QGridLayout *backgroundLayout = new QGridLayout;
    backgroundLayout->addWidget(createBackgroundCellWidget(tr("Blue Grid"),
                                                           ":/images/background1.png"), 0, 0);
    backgroundLayout->addWidget(createBackgroundCellWidget(tr("White Grid"),
                                                           ":/images/background2.png"), 0, 1);
    backgroundLayout->addWidget(createBackgroundCellWidget(tr("Gray Grid"),
                                                           ":/images/background3.png"), 1, 0);
    backgroundLayout->addWidget(createBackgroundCellWidget(tr("No Grid"),
                                                           ":/images/background4.png"), 1, 1);

    backgroundLayout->setRowStretch(2, 10);
    backgroundLayout->setColumnStretch(2, 10);

    QWidget *backgroundWidget = new QWidget;
    backgroundWidget->setLayout(backgroundLayout);

    toolBox = new QToolBox;
    toolBox->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored));
    toolBox->setMinimumWidth(itemWidget->sizeHint().width());
    toolBox->addItem(itemWidget, tr("Basic Flowchart Shapes"));
    toolBox->addItem(backgroundWidget, tr("Backgrounds"));
}

void MainWindow::createActions()
{  
    toFrontAction = new QAction(QIcon(":/images/bringtofront.png"),
                                tr("Bring to &Front"), this);
    toFrontAction->setShortcut(tr("Ctrl+F"));
    toFrontAction->setStatusTip(tr("Bring item to front"));
    connect(toFrontAction, SIGNAL(triggered()), this, SLOT(bringToFront()));

    sendBackAction = new QAction(QIcon(":/images/sendtoback.png"), tr("Send to &Back"), this);
    sendBackAction->setShortcut(tr("Ctrl+T"));
    sendBackAction->setStatusTip(tr("Send item to back"));
    connect(sendBackAction, SIGNAL(triggered()), this, SLOT(sendToBack()));

    deleteAction = new QAction(QIcon(":/images/delete.png"), tr("&Delete"), this);
    deleteAction->setShortcut(tr("Delete"));
    deleteAction->setStatusTip(tr("Delete item from diagram"));
    connect(deleteAction, SIGNAL(triggered()), this, SLOT(deleteItem()));

    convertAction = new QAction(QIcon(":/images/play.png"), tr("&Convert"), this);
    convertAction->setShortcut(tr("Convert"));
    convertAction->setStatusTip(tr("Convert the ASM to Verilog"));
    connect(convertAction, SIGNAL(triggered()), this, SLOT(convert()));

    enlargeShapeAction = new QAction(QIcon(":/images/expand.png"), tr("&Enlarge"), this);
    enlargeShapeAction->setShortcut(tr("Enlarge"));
    enlargeShapeAction->setStatusTip(tr("enlarge the shape"));
    connect(enlargeShapeAction, SIGNAL(triggered()), this, SLOT(enlargeShape()));

    shrinkShapeAction = new QAction(QIcon(":/images/collapse.png"), tr("&Shrink"), this);
    shrinkShapeAction->setShortcut(tr("Shrink"));
    shrinkShapeAction->setStatusTip(tr("shrink the shape"));
    connect(shrinkShapeAction, SIGNAL(triggered()), this, SLOT(shrinkShape()));

    exitAction = new QAction(tr("E&xit"), this);
    exitAction->setShortcuts(QKeySequence::Quit);
    exitAction->setStatusTip(tr("Quit Scenediagram example"));
    connect(exitAction, SIGNAL(triggered()), this, SLOT(close()));

    boldAction = new QAction(tr("Bold"), this);
    boldAction->setCheckable(true);
    QPixmap pixmap(":/images/bold.png");
    boldAction->setIcon(QIcon(pixmap));
    boldAction->setShortcut(tr("Ctrl+B"));
    connect(boldAction, SIGNAL(triggered()), this, SLOT(handleFontChange()));

    italicAction = new QAction(QIcon(":/images/italic.png"), tr("Italic"), this);
    italicAction->setCheckable(true);
    italicAction->setShortcut(tr("Ctrl+I"));
    connect(italicAction, SIGNAL(triggered()), this, SLOT(handleFontChange()));

    underlineAction = new QAction(QIcon(":/images/underline.png"), tr("Underline"), this);
    underlineAction->setCheckable(true);
    underlineAction->setShortcut(tr("Ctrl+U"));
    connect(underlineAction, SIGNAL(triggered()), this, SLOT(handleFontChange()));

    aboutAction = new QAction(tr("A&bout"), this);
    aboutAction->setShortcut(tr("F1"));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(about()));
}

void MainWindow::createMenus()
{
    fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(exitAction);

    itemMenu = menuBar()->addMenu(tr("&Item"));
    itemMenu->addAction(deleteAction);
    itemMenu->addAction(enlargeShapeAction);
    itemMenu->addAction(shrinkShapeAction);
    itemMenu->addSeparator();
    itemMenu->addAction(toFrontAction);
    itemMenu->addAction(sendBackAction);

    aboutMenu = menuBar()->addMenu(tr("&Help"));
    aboutMenu->addAction(aboutAction);
}

void MainWindow::createToolbars()
{
    editToolBar = addToolBar(tr("Edit"));
//    editToolBar->addAction(deleteAction);
    editToolBar->addAction(enlargeShapeAction);
    editToolBar->addAction(shrinkShapeAction);
//    editToolBar->addAction(toFrontAction);
//    editToolBar->addAction(sendBackAction);

    fontCombo = new QFontComboBox();
    connect(fontCombo, SIGNAL(currentFontChanged(QFont)),
            this, SLOT(currentFontChanged(QFont)));

    fontSizeCombo = new QComboBox;
    fontSizeCombo->setEditable(true);
    for (int i = 8; i < 30; i = i + 2)
        fontSizeCombo->addItem(QString().setNum(i));
    QIntValidator *validator = new QIntValidator(2, 64, this);
    fontSizeCombo->setValidator(validator);
    connect(fontSizeCombo, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(fontSizeChanged(QString)));

    fontColorToolButton = new QToolButton;
    fontColorToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    fontColorToolButton->setMenu(createColorMenu(SLOT(textColorChanged()), Qt::black));
    textAction = fontColorToolButton->menu()->defaultAction();
    fontColorToolButton->setIcon(createColorToolButtonIcon(":/images/textpointer.png", Qt::black));
    fontColorToolButton->setAutoFillBackground(true);
    connect(fontColorToolButton, SIGNAL(clicked()),
            this, SLOT(textButtonTriggered()));

    fillColorToolButton = new QToolButton;
    fillColorToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    fillColorToolButton->setMenu(createColorMenu(SLOT(itemColorChanged()), Qt::white));
    fillAction = fillColorToolButton->menu()->defaultAction();
    fillColorToolButton->setIcon(createColorToolButtonIcon(
                                     ":/images/floodfill.png", Qt::white));
    connect(fillColorToolButton, SIGNAL(clicked()),
            this, SLOT(fillButtonTriggered()));

    lineColorToolButton = new QToolButton;
    lineColorToolButton->setPopupMode(QToolButton::MenuButtonPopup);
    lineColorToolButton->setMenu(createColorMenu(SLOT(lineColorChanged()), Qt::black));
    lineAction = lineColorToolButton->menu()->defaultAction();
    lineColorToolButton->setIcon(createColorToolButtonIcon(
                                     ":/images/linecolor.png", Qt::black));
    connect(lineColorToolButton, SIGNAL(clicked()),
            this, SLOT(lineButtonTriggered()));

    textToolBar = addToolBar(tr("Font"));
    textToolBar->addWidget(fontCombo);
    textToolBar->addWidget(fontSizeCombo);
//    textToolBar->addAction(boldAction);
//    textToolBar->addAction(italicAction);
//    textToolBar->addAction(underlineAction);

//    colorToolBar = addToolBar(tr("Color"));
//    colorToolBar->addWidget(fontColorToolButton);
//    colorToolBar->addWidget(fillColorToolButton);
//    colorToolBar->addWidget(lineColorToolButton);

    QToolButton *pointerButton = new QToolButton;
    pointerButton->setCheckable(true);
    pointerButton->setChecked(true);
    pointerButton->setIcon(QIcon(":/images/pointer.png"));
    QToolButton *linePointerButton = new QToolButton;
    linePointerButton->setCheckable(true);
    linePointerButton->setIcon(QIcon(":/images/linepointer.png"));

    pointerTypeGroup = new QButtonGroup(this);
    pointerTypeGroup->addButton(pointerButton, int(DiagramScene::MoveItem));
    pointerTypeGroup->addButton(linePointerButton, int(DiagramScene::InsertLine));
    connect(pointerTypeGroup, SIGNAL(buttonClicked(int)),
            this, SLOT(pointerGroupClicked(int)));

    sceneScaleCombo = new QComboBox;
    QStringList scales;
    scales << tr("50%") << tr("75%") << tr("100%") << tr("125%") << tr("150%");
    sceneScaleCombo->addItems(scales);
    sceneScaleCombo->setCurrentIndex(2);
    connect(sceneScaleCombo, SIGNAL(currentIndexChanged(QString)),
            this, SLOT(sceneScaleChanged(QString)));

    pointerToolbar = addToolBar(tr("Pointer type"));
    pointerToolbar->addWidget(pointerButton);
    pointerToolbar->addWidget(linePointerButton);
    pointerToolbar->addWidget(sceneScaleCombo);
    pointerToolbar->addSeparator();
    pointerToolbar->addAction(convertAction);
}

QWidget *MainWindow::createBackgroundCellWidget(const QString &text, const QString &image)
{
    QToolButton *button = new QToolButton;
    button->setText(text);
    button->setIcon(QIcon(image));
    button->setIconSize(QSize(50, 50));
    button->setCheckable(true);
    backgroundButtonGroup->addButton(button);

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(button, 0, 0, Qt::AlignHCenter);
    layout->addWidget(new QLabel(text), 1, 0, Qt::AlignCenter);

    QWidget *widget = new QWidget;
    widget->setLayout(layout);

    return widget;
}

QWidget *MainWindow::createCellWidget(const QString &text, DiagramItem::DiagramType type)
{

    DiagramItem item(type, itemMenu);
    QIcon icon(item.image());

    QToolButton *button = new QToolButton;
    button->setIcon(icon);
    button->setIconSize(QSize(50, 50));
    button->setCheckable(true);
    buttonGroup->addButton(button, int(type));

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(button, 0, 0, Qt::AlignHCenter);
    layout->addWidget(new QLabel(text), 1, 0, Qt::AlignCenter);

    QWidget *widget = new QWidget;
    widget->setLayout(layout);

    return widget;
}

QMenu *MainWindow::createColorMenu(const char *slot, QColor defaultColor)
{
    QList<QColor> colors;
    colors << Qt::black << Qt::white << Qt::red << Qt::blue << Qt::yellow;
    QStringList names;
    names << tr("black") << tr("white") << tr("red") << tr("blue")
          << tr("yellow");

    QMenu *colorMenu = new QMenu(this);
    for (int i = 0; i < colors.count(); ++i) {
        QAction *action = new QAction(names.at(i), this);
        action->setData(colors.at(i));
        action->setIcon(createColorIcon(colors.at(i)));
        connect(action, SIGNAL(triggered()), this, slot);
        colorMenu->addAction(action);
        if (colors.at(i) == defaultColor)
            colorMenu->setDefaultAction(action);
    }
    return colorMenu;
}

QIcon MainWindow::createColorToolButtonIcon(const QString &imageFile, QColor color)
{
    QPixmap pixmap(50, 80);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QPixmap image(imageFile);
    // Draw icon centred horizontally on button.
    QRect target(4, 0, 42, 43);
    QRect source(0, 0, 42, 43);
    painter.fillRect(QRect(0, 60, 50, 80), color);
    painter.drawPixmap(target, image, source);

    return QIcon(pixmap);
}

QIcon MainWindow::createColorIcon(QColor color)
{
    QPixmap pixmap(20, 20);
    QPainter painter(&pixmap);
    painter.setPen(Qt::NoPen);
    painter.fillRect(QRect(0, 0, 20, 20), color);

    return QIcon(pixmap);
}
