#include "vtextedit.h"

#include <QDebug>
#include <QScrollBar>
#include <QPainter>
#include <QResizeEvent>

#include "vtextdocumentlayout.h"
#include "vimageresourcemanager2.h"


enum class BlockState
{
    Normal = 0,
    CodeBlockStart,
    CodeBlock,
    CodeBlockEnd,
    Comment
};


VTextEdit::VTextEdit(QWidget *p_parent)
    : QTextEdit(p_parent),
      m_imageMgr(nullptr)
{
    init();
}

VTextEdit::VTextEdit(const QString &p_text, QWidget *p_parent)
    : QTextEdit(p_text, p_parent),
      m_imageMgr(nullptr)
{
    init();
}

VTextEdit::~VTextEdit()
{
    if (m_imageMgr) {
        delete m_imageMgr;
    }
}

void VTextEdit::init()
{
    m_lineNumberType = LineNumberType::None;

    m_blockImageEnabled = false;

    m_imageMgr = new VImageResourceManager2();

    QTextDocument *doc = document();
    VTextDocumentLayout *docLayout = new VTextDocumentLayout(doc, m_imageMgr);
    docLayout->setBlockImageEnabled(m_blockImageEnabled);
    doc->setDocumentLayout(docLayout);

    m_lineNumberArea = new VLineNumberArea(this,
                                           document(),
                                           fontMetrics().width(QLatin1Char('8')),
                                           fontMetrics().height(),
                                           this);
    connect(doc, &QTextDocument::blockCountChanged,
            this, &VTextEdit::updateLineNumberAreaMargin);
    connect(this, &QTextEdit::textChanged,
            this, &VTextEdit::updateLineNumberArea);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &VTextEdit::updateLineNumberArea);
    connect(this, &QTextEdit::cursorPositionChanged,
            this, &VTextEdit::updateLineNumberArea);
}

VTextDocumentLayout *VTextEdit::getLayout() const
{
    return qobject_cast<VTextDocumentLayout *>(document()->documentLayout());
}

void VTextEdit::setLineLeading(qreal p_leading)
{
    getLayout()->setLineLeading(p_leading);
}

void VTextEdit::resizeEvent(QResizeEvent *p_event)
{
    QTextEdit::resizeEvent(p_event);

    if (m_lineNumberType != LineNumberType::None) {
        QRect rect = contentsRect();
        m_lineNumberArea->setGeometry(QRect(rect.left(),
                                            rect.top(),
                                            m_lineNumberArea->calculateWidth(),
                                            rect.height()));
    }
}

void VTextEdit::paintLineNumberArea(QPaintEvent *p_event)
{
    if (m_lineNumberType == LineNumberType::None) {
        updateLineNumberAreaMargin();
        m_lineNumberArea->hide();
        return;
    }

    QPainter painter(m_lineNumberArea);
    painter.fillRect(p_event->rect(), m_lineNumberArea->getBackgroundColor());

    QTextBlock block = firstVisibleBlock();
    if (!block.isValid()) {
        return;
    }

    VTextDocumentLayout *layout = getLayout();
    Q_ASSERT(layout);

    int blockNumber = block.blockNumber();
    QRectF rect = layout->blockBoundingRect(block);
    int top = contentOffsetY() + (int)rect.y();
    int bottom = top + (int)rect.height();
    int eventTop = p_event->rect().top();
    int eventBtm = p_event->rect().bottom();
    const int digitHeight = m_lineNumberArea->getDigitHeight();
    const int curBlockNumber = textCursor().block().blockNumber();
    painter.setPen(m_lineNumberArea->getForegroundColor());
    const int leading = (int)layout->getLineLeading();

    // Display line number only in code block.
    if (m_lineNumberType == LineNumberType::CodeBlock) {
        int number = 0;
        while (block.isValid() && top <= eventBtm) {
            int blockState = block.userState();
            switch (blockState) {
            case (int)BlockState::CodeBlockStart:
                Q_ASSERT(number == 0);
                number = 1;
                break;

            case (int)BlockState::CodeBlockEnd:
                number = 0;
                break;

            case (int)BlockState::CodeBlock:
                if (number == 0) {
                    // Need to find current line number in code block.
                    QTextBlock startBlock = block.previous();
                    while (startBlock.isValid()) {
                        if (startBlock.userState() == (int)BlockState::CodeBlockStart) {
                            number = block.blockNumber() - startBlock.blockNumber();
                            break;
                        }

                        startBlock = startBlock.previous();
                    }
                }

                break;

            default:
                break;
            }

            if (blockState == (int)BlockState::CodeBlock) {
                if (block.isVisible() && bottom >= eventTop) {
                    QString numberStr = QString::number(number);
                    painter.drawText(0,
                                     top + leading,
                                     m_lineNumberArea->width(),
                                     digitHeight,
                                     Qt::AlignRight,
                                     numberStr);
                }

                ++number;
            }

            block = block.next();
            top = bottom;
            bottom = top + (int)layout->blockBoundingRect(block).height();
        }

        return;
    }

    // Handle m_lineNumberType 1 and 2.
    Q_ASSERT(m_lineNumberType == LineNumberType::Absolute
             || m_lineNumberType == LineNumberType::Relative);
    while (block.isValid() && top <= eventBtm) {
        if (block.isVisible() && bottom >= eventTop) {
            bool currentLine = false;
            int number = blockNumber + 1;
            if (m_lineNumberType == LineNumberType::Relative) {
                number = blockNumber - curBlockNumber;
                if (number == 0) {
                    currentLine = true;
                    number = blockNumber + 1;
                } else if (number < 0) {
                    number = -number;
                }
            } else if (blockNumber == curBlockNumber) {
                currentLine = true;
            }

            QString numberStr = QString::number(number);

            if (currentLine) {
                QFont font = painter.font();
                font.setBold(true);
                painter.setFont(font);
            }

            painter.drawText(0,
                             top + leading,
                             m_lineNumberArea->width(),
                             digitHeight,
                             Qt::AlignRight,
                             numberStr);

            if (currentLine) {
                QFont font = painter.font();
                font.setBold(false);
                painter.setFont(font);
            }
        }

        block = block.next();
        top = bottom;
        bottom = top + (int)layout->blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void VTextEdit::updateLineNumberAreaMargin()
{
    int width = 0;
    if (m_lineNumberType != LineNumberType::None) {
        width = m_lineNumberArea->calculateWidth();
    }

    if (width != viewportMargins().left()) {
        setViewportMargins(width, 0, 0, 0);
    }
}

void VTextEdit::updateLineNumberArea()
{
    if (m_lineNumberType != LineNumberType::None) {
        if (!m_lineNumberArea->isVisible()) {
            updateLineNumberAreaMargin();
            m_lineNumberArea->show();
        }

        m_lineNumberArea->update();
    } else if (m_lineNumberArea->isVisible()) {
        updateLineNumberAreaMargin();
        m_lineNumberArea->hide();
    }
}

QTextBlock VTextEdit::firstVisibleBlock() const
{
    VTextDocumentLayout *layout = getLayout();
    Q_ASSERT(layout);
    int blockNumber = layout->findBlockByPosition(QPointF(0, -contentOffsetY()));
    return document()->findBlockByNumber(blockNumber);
}

int VTextEdit::contentOffsetY() const
{
    QScrollBar *sb = verticalScrollBar();
    return -(sb->value());
}

void VTextEdit::updateBlockImages(const QVector<VBlockImageInfo2> &p_blocksInfo)
{
    if (m_blockImageEnabled) {
        m_imageMgr->updateBlockInfos(p_blocksInfo);
    }
}

void VTextEdit::clearBlockImages()
{
    m_imageMgr->clear();
}

bool VTextEdit::containsImage(const QString &p_imageName) const
{
    return m_imageMgr->contains(p_imageName);
}

void VTextEdit::addImage(const QString &p_imageName, const QPixmap &p_image)
{
    if (m_blockImageEnabled) {
        m_imageMgr->addImage(p_imageName, p_image);
    }
}

void VTextEdit::setBlockImageEnabled(bool p_enabled)
{
    if (m_blockImageEnabled == p_enabled) {
        return;
    }

    m_blockImageEnabled = p_enabled;

    getLayout()->setBlockImageEnabled(m_blockImageEnabled);

    if (!m_blockImageEnabled) {
        clearBlockImages();
    }
}

void VTextEdit::setImageWidthConstrainted(bool p_enabled)
{
    getLayout()->setImageWidthConstrainted(p_enabled);
}
