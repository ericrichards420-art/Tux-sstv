#include "mainwindow.h"
#include <QtWidgets>
#include <QUndoStack>
#include <QUndoCommand>

namespace {
class TxEditCommand final:public QUndoCommand {
public:
    TxEditCommand(TxEditState before,TxEditState after,const QString& label,std::function<void(const TxEditState&)> restore)
        :QUndoCommand(label),before_(std::move(before)),after_(std::move(after)),restore_(std::move(restore)){}
    void undo() override{restore_(before_);}
    void redo() override{
        // The UI already applied a freshly committed edit. Only redo after Undo
        // restores a snapshot; pushing must not render twice or interrupt a drag.
        if(firstRedo_){firstRedo_=false;return;}
        restore_(after_);
    }
private:
    TxEditState before_,after_;
    std::function<void(const TxEditState&)> restore_;
    bool firstRedo_=true;
};
}
TxEditState MainWindow::txEditState() const{
    return {source_,inset_,callsignPlacement_,captions_,templateFields_,templateEnabled_,showCallsignOnImage_->isChecked(),selectedPattern_};
}
void MainWindow::restoreTxEditState(const TxEditState& state){
    source_=state.source;inset_=state.inset;callsignPlacement_=state.callsignPlacement;captions_=state.captions;
    templateFields_=state.templateFields;templateEnabled_=state.templateEnabled;selectedPattern_=state.selectedPattern;
    {const QSignalBlocker block(showCallsignOnImage_);showCallsignOnImage_->setChecked(state.showCallsign);}
    overlay_.text.clear();overlay_.freePosition=false;overlay_.relativePosition={};
    saveSettings();updateTx();
    if(!txFullScreen_->isVisible())viewerTabs_->setCurrentWidget(txPage_);
}
void MainWindow::recordTxEdit(const TxEditState& before,const QString& label){
    pushTxEdit(before,txEditState(),label);
}
void MainWindow::pushTxEdit(const TxEditState& before,const TxEditState& after,const QString& label){
    if(before==after)return;
    txUndo_->push(new TxEditCommand(before,after,label,[this](const TxEditState& state){restoreTxEditState(state);}));
    updateTxUndoActions();
}
void MainWindow::beginTxDrag(const QString& key){
    if(!canMoveTxText()||txDragBefore_)return;
    txDragBefore_=txEditState();txDragLabel_=key=="inset"?"Move received picture":key=="callsign"?"Move callsign":"Move text";updateTxUndoActions();
}
void MainWindow::finishTxDrag(){
    if(!txDragBefore_)return;
    const auto before=std::move(*txDragBefore_);txDragBefore_.reset();auto after=txEditState();after.showCallsign=before.showCallsign;pushTxEdit(before,after,txDragLabel_);txDragLabel_.clear();updateTxUndoActions();
}
void MainWindow::resetTxHistory(){txDragBefore_.reset();txDragLabel_.clear();txUndo_->clear();updateTxUndoActions();}
void MainWindow::updateTxUndoActions(){
    if(!undoTxAction_||!redoTxAction_)return;
    const bool editable=!closing_&&canMoveTxText()&&!txDragBefore_;
    undoTxAction_->setText(txUndo_->canUndo()?"Undo "+txUndo_->undoText():"Undo");redoTxAction_->setText(txUndo_->canRedo()?"Redo "+txUndo_->redoText():"Redo");
    undoTxAction_->setEnabled(editable&&txUndo_->canUndo());redoTxAction_->setEnabled(editable&&txUndo_->canRedo());
    undoTxButton_->setEnabled(undoTxAction_->isEnabled());redoTxButton_->setEnabled(redoTxAction_->isEnabled());
    undoTxButton_->setToolTip(undoTxAction_->text()+" (Ctrl+Z)");redoTxButton_->setToolTip(redoTxAction_->text()+" (Ctrl+Shift+Z)");
}
void MainWindow::undoTx(){if(!closing_&&canMoveTxText()&&!txDragBefore_&&!QApplication::activeModalWidget()&&txUndo_->canUndo()){txUndo_->undo();updateTxUndoActions();}}
void MainWindow::redoTx(){if(!closing_&&canMoveTxText()&&!txDragBefore_&&!QApplication::activeModalWidget()&&txUndo_->canRedo()){txUndo_->redo();updateTxUndoActions();}}
