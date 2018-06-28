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
#include <set>
#include <string>

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

        Arrow* nextArrow = NULL ;

        for(auto arrow : current->getArrows()){
            if(arrow->startItem() == current){
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


void find_errors(bool is_root, DiagramItem* item, std::map<DiagramItem*, int>& states,
              std::vector<DiagramItem*> visited_items, std::vector<std::string>& errors){

    if (item->diagramType() == DiagramItem::DiagramType::Step){
        if(not is_root){
            return;
        }
    }

    for(auto visited_item : visited_items){
        if(visited_item == item){
            errors.push_back("* Found a loop without any state boxes.");
            return;
        }
    }

    if(visited_items.size() > 0){
        auto last_item = visited_items[visited_items.size() - 1];

        if(item->diagramType() == DiagramItem::DiagramType::Io and
           last_item->diagramType() == DiagramItem::DiagramType::Io)
        {
            errors.push_back("* Found conditional box after another conditional box.");
            return;
        }

        if(item->diagramType() == DiagramItem::DiagramType::Io and
           last_item->diagramType() == DiagramItem::DiagramType::Step)
        {
            errors.push_back("* Found conditional box after a state box.");
            return;
        }
    }

    auto arrows = item->getArrows();

    for(auto arrow : arrows){
        if(arrow->startItem() == item){
            std::string arrow_value;
            Arrow* last_arrow_piece;
            DiagramItem* arrowDest = findArrowDest(arrow, arrow_value, last_arrow_piece);

            if(not arrowDest){
                errors.push_back("* Found an arrow pointing nowhere.");
                return;
            }

            auto new_visited_states = visited_items;

            new_visited_states.push_back(item);

            find_errors(false, arrowDest, states, new_visited_states, errors);
        }
    }
}


void MainWindow::convert(){

    //finding all the states

    DiagramItem* topMostState = NULL;
    int topMostStateY = 1000000;

    std::map<DiagramItem *, int> states;

    int counter = 0;
    for(auto i : scene->items()){
        if(i->type() == DiagramItem::Type){
            DiagramItem *diagramItem = qgraphicsitem_cast<DiagramItem *>(i);
            if(diagramItem->diagramType() == DiagramItem::DiagramType::Step){
                states.insert(std::make_pair(diagramItem, counter));
                counter ++;

                if(diagramItem->pos().y() < topMostStateY){
                    topMostState = diagramItem;
                    topMostStateY = diagramItem->pos().y();
                }
            }
        }
    }

    int resetStateID = states[topMostState];

    // checking for errors

    std::vector<std::string> errors;

    for(auto state = states.begin(); state != states.end(); state++){
        find_errors(true, state->first, states, std::vector<DiagramItem*>(), errors);
    }

    std::string all_errors;
    for(auto error : errors){
        all_errors += (error + "\n");
    }

    all_errors += "\n\n Please fix your errors and try again.";

    if(errors.size()){
        QMessageBox::critical(
            this,
            "Error in ASM!",
            QString::fromStdString(all_errors));

        return;
    }

    std::string module_name = module_name_input->text().toStdString();

    string fileName = module_name + ".v";

    std::ofstream output_file(fileName);

    set<string> input_output_var;

    set<string> middle_var;

    output_file << "module " << module_name_input->text().toStdString() << "(" ;

    int rowCount = inputs_output_table->rowCount();

    bool first = false;

    for(int i = 0; i < rowCount; i++){
        if(inputs_output_table->item(i, 0) and inputs_output_table->item(i, 2)){

            std::string IOName = inputs_output_table->item(i, 0)->text().toStdString();

            input_output_var.insert(IOName);

            if(first){
                output_file << ", ";
            }
            first = true;

            output_file << IOName;
        }
    }

    output_file << ");\n" << std::endl;

    for(int i = 0; i < rowCount; i++){
        if(inputs_output_table->item(i, 0) and inputs_output_table->item(i, 2)){

            auto index = inputs_output_table->model()->index(i, 1);
            auto widget = inputs_output_table->indexWidget(index);
            QComboBox* comboBox = static_cast<QComboBox*>(widget);
            std::string IOType = comboBox->currentText().toStdString();

            std::string IOName = inputs_output_table->item(i, 0)->text().toStdString();

            int bits = std::stoi(inputs_output_table->item(i, 2)->text().toStdString());

            if(IOType == "input"){
                if(bits > 1)
                    output_file << "input reg[" << bits - 1 << ":0] " << IOName << ";";
                else if(bits == 1)
                    output_file << "input reg " << IOName << ";";
            } else {
                if(bits > 1)
                    output_file << "output wire[" << bits - 1 << ":0] " << IOName << ";";
                else if (bits == 1)
                    output_file << "output wire " << IOName << ";";
            }

            output_file << "\n";
        }
    }

    output_file << "\n";

    std::vector<ASMBlock> asm_blocks;

    for(auto state = states.begin(); state != states.end(); state++){
        ASMBlock asm_block(state->second);

        traverse(true, state->first, NULL, std::vector<condition>(), asm_block, states);

        asm_blocks.push_back(asm_block);
    }

    //declare middle variable
    ///////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////

    for(auto asmBlock : asm_blocks){
        stringstream stateCode(asmBlock.default_code);
        string rt;

        if(asmBlock.default_code != "")
        {
            while(getline(stateCode , rt , '\n'))
            {
                string LHS = rt.substr(0 , rt.find("<=") - 1);
                if(!(input_output_var.find(LHS) != input_output_var.end()))
                    middle_var.insert(LHS);
            }
        }
        for(auto conditionBox : asmBlock.conditional_codes){
            stringstream condCode(conditionBox.code);
            string rt;

            if(conditionBox.code != "")
            {
                while(getline(condCode , rt , '\n'))
                {
                    string LHS = rt.substr(0 , rt.find("<=") - 1);
                    if(!(input_output_var.find(LHS) != input_output_var.end()))
                        middle_var.insert(LHS);
                }
            }
        }

    }

    for(auto i : middle_var)
    {
        output_file << "integer " << i << ";" << endl;
    }

    output_file << "integer state;\n";
    output_file << "\n" ;

    //combo. segment
    ///////////////////////////////////////////////////////////////////
    ///////////////////////////////////////////////////////////////////
    output_file << "always @ (posedge clock)" << endl;
    output_file << "begin" << endl;
    output_file << "if (reset == 1'b1) begin" << endl;
    output_file << "\t state <= " << resetStateID << ";" << endl;

    output_file << "end else" << endl;
    output_file << "case (state)" << endl;

    for(auto asmBlock : asm_blocks){
        output_file <<  asmBlock.id << ":" << "begin" << endl;

        stringstream ss(asmBlock.default_code);
        string rt;

        if(asmBlock.default_code != "")
        {
            while(getline(ss , rt , '\n'))
                output_file << "\t" << rt << ";" << endl;
        }
        bool flag = true;
        for(auto conditionBox : asmBlock.conditional_codes)
        {
            if(flag){
                output_file << "\tif (";
            }
            else {
                output_file << "\telse if(";
            }

            flag = false;

            int condSize = conditionBox.conditions.size();

            if(condSize == 1){
                if(atoi(conditionBox.conditions[0].value.c_str()) == 1){
                    output_file <<  conditionBox.conditions[0].condition ;
                }
                else{
                    output_file << "(!(" << conditionBox.conditions[0].condition << "))";
                }
            }
            else{
                for(int i = 0 ; i < condSize; i++){
                    if(atoi(conditionBox.conditions[i].value.c_str()) == 1)
                        output_file << "(" << conditionBox.conditions[i].condition << ")";
                    else
                        output_file << "(!( " << conditionBox.conditions[i].condition << "))";
                    if (i < condSize - 1)
                        output_file << " && " ;
                }
            }

            output_file << ") begin"<< endl;

            stringstream ss(conditionBox.code);
            string rt;

            if(conditionBox.code != "")
            {
                while(getline(ss , rt , '\n'))
                    output_file << "\t\t" << rt << ";" << endl;
            }

            output_file << "\tend" << endl;
        }

        output_file << "end" << endl;
    }

    output_file << "endcase" << endl;
    output_file << "end" << endl;

    //seq. segment
    //////////////////////////////////////////////////////////////////////////////////////
    //////////////////////////////////////////////////////////////////////////////////////

    //nextState generator

    output_file << "always @ (posedge clock)" << endl;
    output_file << "begin" << endl;
    output_file << "if (reset == 1'b1) begin" << endl;
    output_file << "\t state <= " << resetStateID << ";" << endl;

    output_file << "end else" << endl;
    output_file << "case (state)" << endl;
    for(auto asmBlock : asm_blocks)
    {
        output_file <<  asmBlock.id << ":" << "begin" << endl;
        bool flag = true;
        for(auto stateBox : asmBlock.next_states)
        {
            int stateSize = stateBox.conditions.size();
            if(stateSize > 0)
            {
                if(flag)
                output_file << "\tif (";
                else output_file << "\telse if (";
                flag = false;
            }


            if(stateSize == 1)
            {
                if(atoi(stateBox.conditions[0].value.c_str()) == 1)
                    output_file <<  stateBox.conditions[0].condition ;
                else
                    output_file << "(!(" << stateBox.conditions[0].condition << "))";
            }
            else
            for(int i = 0 ; i < stateSize ; i++)
            {
                if(atoi(stateBox.conditions[i].value.c_str()) == 1)
                    output_file << "(" << stateBox.conditions[i].condition << ")";
                else
                    output_file << "(!(" << stateBox.conditions[i].condition << "))";
                if (i < stateBox.conditions.size() - 1)
                    output_file << " && " ;
            }
            if(stateSize > 0)
            output_file << ")"<< endl;
            output_file << "\t\tstate <= " << stateBox.next_state_id << ";" << endl;
        }
        output_file << "end" << endl;
    }
    output_file << "endcase" << endl;
    output_file << "end" << endl << endl;


    output_file << "endmodule" << std::endl;

    system(("xdg-open " + fileName).c_str());

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

    int numRows = 10;


    module_name_input = new QLineEdit();
    module_name_input->setText("ASM");


    module_name_label = new QLabel(this);
    input_output_table_label = new QLabel(this);
    module_name_label->setText("Module Name : ");
    input_output_table_label->setText("Inputs and Outputs : ");


    inputs_output_table = new QTableWidget();

    inputs_output_table->setRowCount(numRows);
    inputs_output_table->setColumnCount(3);

    for(int i = 0; i < numRows; i++){
        QComboBox *comboBox = new QComboBox;

        comboBox->addItem(tr("input"));
        comboBox->addItem(tr("output"));

        inputs_output_table->setIndexWidget(inputs_output_table->model()->index(i, 1), comboBox);

    }

    inputs_output_table->setHorizontalHeaderLabels(QStringList() << "Name" << "Type" << "Bits");

    backgroundLayout->addWidget(module_name_label);

    backgroundLayout->addWidget(module_name_input);

    backgroundLayout->addWidget(input_output_table_label);

    backgroundLayout->addWidget(inputs_output_table);

    QWidget *backgroundWidget = new QWidget;
    backgroundWidget->setLayout(backgroundLayout);

    toolBox = new QToolBox;
    toolBox->setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Ignored));
    toolBox->setMinimumWidth(300);
    toolBox->addItem(itemWidget, tr("ASM Shapes"));
    toolBox->addItem(backgroundWidget, tr("Inputs / Outputs"));
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

    saveAction = new QAction(tr("&Save") , this);
    saveAction->setShortcut(tr("save file"));
    saveAction->setStatusTip(tr("save ASM file"));
    connect(saveAction , SIGNAL(triggered()) , this , SLOT(saveToFile()));

    loadAction = new QAction(tr("&Load") , this);
    loadAction->setShortcut(tr("load file"));
    loadAction->setStatusTip(tr("load ASM file"));
    connect(loadAction , SIGNAL(triggered()) , this , SLOT(loadFromFile()));

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
    fileMenu->addAction(saveAction);
    fileMenu->addAction(loadAction);

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

void MainWindow::saveToFile(){
    QString fileName = QFileDialog::getSaveFileName(this , tr("Save Address Book"), "",
                                                    tr("Address Book (*.abk);;All Files (*)"));
    if(fileName.isEmpty())
        return;
    else{
        QFile file(fileName);
        if(!file.open(QIODevice::WriteOnly)){
            QMessageBox::information(this , tr("Unable to open file"), file.errorString());
            return;
        }
        QDataStream out(&file);
        out.setVersion(QDataStream::Qt_4_5);
        for(auto i:scene->items()){
            if(i->type() == DiagramItem::Type){
                DiagramItem *diagramItem = qgraphicsitem_cast<DiagramItem *>(i);
                out << diagramItem->type() << diagramItem->diagramType();
            }
            else if(i->type() == Arrow::Type)
                cout << "Arrow" << endl;
            else if(i->type() == DiagramTextItem::Type)
                cout << "TextItem" << endl;
        }
    }
}

void MainWindow::loadFromFile(){
    QString fileName = QFileDialog::getOpenFileName(this , tr("Open Address Book"), "",
                                                    tr("Address Book (*.abk);;All Files (*)"));
    if (fileName.isEmpty())
            return;
        else {

            QFile file(fileName);

            if (!file.open(QIODevice::ReadOnly)) {
                QMessageBox::information(this, tr("Unable to open file"),
                    file.errorString());
                return;
            }

            QDataStream in(&file);
            in.setVersion(QDataStream::Qt_4_5);

        }
}
