#pragma once

#include "InputFile.h"

#include <QAbstractTableModel>

class InputFileModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    InputFileModel(InputFile* pFile, QObject* parent = nullptr);


    Qt::ItemFlags flags(const QModelIndex& index) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;
    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;

    static void writeFileOnDisk(InputFile* pInputFile);


    void inline setCellClicked(bool bClicked) { m_bCellClicked = bClicked; }

    inline void beginReset() { beginResetModel(); }
    inline void endReset() { endResetModel(); }

    inline void setTemplateRow(int row) { m_iTemplateRow = row; }

private:
    void inline setCachedFileData(int rowIdx, int colIdx, QString val);
    void addToStack(InputFile::CellEditAction action);
    void addToStackWithNonEmptyRedo(InputFile::CellEditAction action);
    void updateActionMenus();

    InputFile* m_pFile;
    bool m_bCellClicked;
    int m_iTemplateRow;
};
