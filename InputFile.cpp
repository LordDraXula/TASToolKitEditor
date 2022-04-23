#include "InputFile.h"
#include "InputFileModel.h"

#include <QAction>
#include <QFile>
#include <QFileSystemWatcher>
#include <QLabel>
#include <QMenu>
#include <QTableView>
#include <QTextStream>

#define INVALID_IDX -1

CellEditAction::CellEditAction()
    : m_rowIdx(INVALID_IDX)
    , m_colIdx(INVALID_IDX)
    , m_prev("")
    , m_cur("")
{
}

CellEditAction::CellEditAction(int row, int col, QString prev, QString cur)
    : m_rowIdx(row)
    , m_colIdx(col)
    , m_prev(prev)
    , m_cur(cur)
{
}

bool CellEditAction::operator==(const CellEditAction& rhs)
{
    return m_rowIdx == rhs.m_rowIdx && m_colIdx == rhs.m_colIdx && m_cur == rhs.m_cur;
}

InputFile::InputFile(const InputFileMenus& menus, QLabel* label, QTableView* tableView)
    : m_filePath("")
    , m_fileCentering(Centering::Unknown)
    , m_tableViewLoaded(false)
    , pTableView(tableView)
    , m_menus(menus)
    , pLabel(label)
    , m_frameParseError(INVALID_IDX)
    , m_pFsWatcher(nullptr)
    , m_bModified(false)
    , m_iModifiedCheck(0)
{
}

FileStatus InputFile::loadFile(QString path)
{
    m_filePath = path;

    QFile fp(m_filePath);
    if (!fp.open(QIODevice::ReadWrite))
        return FileStatus::WritePermission;

    QTextStream ts(&fp);

    while (!ts.atEnd())
    {
        QString line = ts.readLine();
        QStringList frameData = line.split(',');

        if (!valuesFormattedProperly(frameData))
        {
            m_frameParseError = m_fileData.count() + 1;
            clearData();
            return FileStatus::Parse;
        }

        m_fileData.append(frameData.toVector());
    }

    m_pFsWatcher = new QFileSystemWatcher(QStringList(path));

    return FileStatus::Success;
}

void InputFile::onCellClicked(const QModelIndex& index)
{
    // Only care about the button columns
    if (index.column() < 1 || index.column() > 3)
        return;

    // Get current value
    QVariant prevVal = pTableView->model()->data(index, Qt::CheckStateRole);
    QVariant newVal = (prevVal == Qt::Checked) ? Qt::Unchecked : Qt::Checked;
    
    ((InputFileModel*) pTableView->model())->setCellClicked(true);
    pTableView->model()->setData(index, newVal, Qt::EditRole);
}

void InputFile::fileChanged()
{
    // QFileSystemWatcher will send the event when our own program modifies the file
    // But it will do so twice for some reason... So prevent this function from running twice
    if (m_bModified)
    {
        if (++m_iModifiedCheck == 2)
        {
            m_bModified = false;
            m_iModifiedCheck = 0;
        }
        
        return;
    }

    m_fileData.clear();
    loadFile(m_filePath);
}

void InputFile::clearData()
{
    m_filePath = "";
    m_fileData.clear();
}

bool InputFile::valuesFormattedProperly(const QStringList& data)
{
    // There should be 6 comma-separated values per line
    if (data.count() != NUM_INPUT_COLUMNS)
        return false;

    // Certain columns have restricted values
    if (!valueRestrictionsAreMet(data))
        return false;

    // Place other error checks here

    return true;
}

bool InputFile::valueRestrictionsAreMet(const QStringList& data)
{
    for (int i = 0; i < data.count(); i++)
    {
        // -0 actually succeeds in the toInt() conversion
        // so we need to manually catch it, since this value
        // won't work when used in Dolphin
        if (data[i] == "-0")
            return false;

        bool ret;
        int value = data[i].toInt(&ret);

        if (!ret)
            return false;

        int smallestAcceptedVal = getSmallestAcceptedValue(i, value);
        int largestAcceptedVal = getLargestAcceptedValue(i, value);

        if (value > largestAcceptedVal || value < smallestAcceptedVal)
            return false;
    }

    return true;
}

int InputFile::getSmallestAcceptedValue(int index, int value)
{
    if (BUTTON_COL_IDXS.contains(index) || DPAD_COL_IDX == index)
        return 0;

    if (m_fileCentering == Centering::Unknown && !ableToDiscernCentering(value))
        return 0;

    return m_fileCentering == Centering::Seven ? 0 : -7;
}

int InputFile::getLargestAcceptedValue(int index, int value)
{
    if (DPAD_COL_IDX == index)
        return 4;
    if (BUTTON_COL_IDXS.contains(index))
        return 1;
    if (m_fileCentering == Centering::Unknown && !ableToDiscernCentering(value))
        return 7;

    return m_fileCentering == Centering::Seven ? 14 : 7;
}

bool InputFile::ableToDiscernCentering(int value)
{
    if (value > 7)
        m_fileCentering = Centering::Seven;
    else if (value < 0)
        m_fileCentering = Centering::Zero;
    else
        return false;

    return true;
}


void InputFile::closeFile()
{
    clearData();
    m_fileCentering = Centering::Unknown;
    delete m_pFsWatcher;
    pLabel->setVisible(false);
    pTableView->setVisible(false);
    m_menus.root->menuAction()->setVisible(false);
    m_menus.close->setEnabled(false);
}

bool InputFile::inputValid(const QModelIndex& index, const QVariant& value)
{
    if (value == "")
        return false;

    bool bToFloat = false;

    int iValue = (int)value.toFloat(&bToFloat);

    // Check if conversion fails (user entered invalid chars)
    if (!bToFloat)
        return false;

    if (BUTTON_COL_IDXS.contains(index.column() - FRAMECOUNT_COLUMN))
        return (iValue == 0 || iValue == 1);
    if (index.column() == DPAD_COL_IDX + FRAMECOUNT_COLUMN)
        return (iValue >= 0 && iValue <= 4);
    
    if (m_fileCentering == Centering::Seven)
        return (iValue >= 0 && iValue <= 14);
    if (m_fileCentering == Centering::Zero)
        return (iValue >= -7 && iValue <= 7);
    
    // File centering is unknown... check extreme bounds
    if (iValue < -7 || iValue > 14)
        return false;

    ableToDiscernCentering(iValue);
    return true;
}

void InputFile::swap(InputFile* rhs)
{
    std::swap(m_fileData, rhs->m_fileData);
    std::swap(m_fileCentering, rhs->m_fileCentering);
    std::swap(m_undoStack, rhs->m_undoStack);
    std::swap(m_redoStack, rhs->m_redoStack);
    std::swap(pTableView, rhs->pTableView);
    std::swap(m_pFsWatcher, rhs->m_pFsWatcher);

    InputFileModel::writeFileOnDisk(this);
    InputFileModel::writeFileOnDisk(rhs);

    this->getTableView()->viewport()->update();
    rhs->getTableView()->viewport()->update();
}