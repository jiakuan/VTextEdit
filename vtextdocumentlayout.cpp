#include "vtextdocumentlayout.h"

#include <QTextDocument>
#include <QTextBlock>
#include <QTextFrame>
#include <QTextLayout>
#include <QPointF>
#include <QFontMetrics>
#include <QFont>
#include <QPainter>
#include <QDebug>


VTextDocumentLayout::VTextDocumentLayout(QTextDocument *p_doc)
    : QAbstractTextDocumentLayout(p_doc),
      m_pageWidth(0),
      m_margin(p_doc->documentMargin()),
      m_width(0),
      m_maximumWidthBlockNumber(-1),
      m_height(0),
      m_blockCount(0),
      m_cursorWidth(1)
{
}

static void fillBackground(QPainter *p_painter,
                           const QRectF &p_rect,
                           QBrush p_brush,
                           QRectF p_gradientRect = QRectF())
{
    p_painter->save();
    if (p_brush.style() >= Qt::LinearGradientPattern
        && p_brush.style() <= Qt::ConicalGradientPattern) {
        if (!p_gradientRect.isNull()) {
            QTransform m = QTransform::fromTranslate(p_gradientRect.left(),
                                                     p_gradientRect.top());
            m.scale(p_gradientRect.width(), p_gradientRect.height());
            p_brush.setTransform(m);
            const_cast<QGradient *>(p_brush.gradient())->setCoordinateMode(QGradient::LogicalMode);
        }
    } else {
        p_painter->setBrushOrigin(p_rect.topLeft());
    }

    p_painter->fillRect(p_rect, p_brush);
    p_painter->restore();
}

void VTextDocumentLayout::blockRangeFromRect(const QRectF &p_rect,
                                             int &p_first,
                                             int &p_last) const
{
    if (p_rect.isNull()) {
        p_first = 0;
        p_last = m_blocks.size() - 1;
        return;
    }

    p_first = -1;
    p_last = m_blocks.size() - 1;
    int y = p_rect.y();
    Q_ASSERT(document()->blockCount() == m_blocks.size());
    QTextBlock block = document()->firstBlock();
    while (block.isValid()) {
        const BlockInfo &info = m_blocks[block.blockNumber()];
        Q_ASSERT(info.hasOffset());

        if (info.top() == y
            || (info.top() < y && info.bottom() >= y)) {
            p_first = block.blockNumber();
            break;
        }

        block = block.next();
    }

    if (p_first == -1) {
        p_last = -1;
        return;
    }

    y += p_rect.height();
    while (block.isValid()) {
        const BlockInfo &info = m_blocks[block.blockNumber()];
        Q_ASSERT(info.hasOffset());

        if (info.bottom() > y) {
            p_last = block.blockNumber();
            break;
        }

        block = block.next();
    }

    qDebug() << "block range" << p_first << p_last;
}

void VTextDocumentLayout::draw(QPainter *p_painter, const PaintContext &p_context)
{
    qDebug() << "VTextDocumentLayout draw()" << p_context.clip << p_context.cursorPosition << p_context.selections.size();

    // Find out the blocks.
    int first, last;
    blockRangeFromRect(p_context.clip, first, last);
    if (first == -1) {
        return;
    }

    QTextDocument *doc = document();
    Q_ASSERT(doc->blockCount() == m_blocks.size());
    QPointF offset(m_margin, m_blocks[first].top());
    QTextBlock block = doc->findBlockByNumber(first);
    QTextBlock lastBlock = doc->findBlockByNumber(last);

    QPen oldPen = p_painter->pen();
    p_painter->setPen(p_context.palette.color(QPalette::Text));

    while (block.isValid()) {
        const BlockInfo &info = m_blocks[block.blockNumber()];
        Q_ASSERT(info.hasOffset());

        const QRectF &rect = info.m_rect;
        QTextLayout *layout = block.layout();

        if (!block.isVisible()) {
            offset.ry() += rect.height();
            if (block == lastBlock) {
                break;
            }

            block = block.next();
            continue;
        }

        QTextBlockFormat blockFormat = block.blockFormat();
        QBrush bg = blockFormat.background();
        if (bg != Qt::NoBrush) {
            fillBackground(p_painter, rect, bg);
        }

        auto selections = formatRangeFromSelection(block, p_context.selections);

        layout->draw(p_painter,
                     offset,
                     selections,
                     p_context.clip.isValid() ? p_context.clip : QRectF());

        // Draw the cursor.
        int blpos = block.position();
        int bllen = block.length();
        bool drawCursor = p_context.cursorPosition >= blpos
                          && p_context.cursorPosition < blpos + bllen;
        if (drawCursor
            || (p_context.cursorPosition < -1
                && !layout->preeditAreaText().isEmpty())) {
            int cpos = p_context.cursorPosition;
            if (cpos < -1) {
                cpos = layout->preeditAreaPosition() - (cpos + 2);
            } else {
                cpos -= blpos;
            }

            layout->drawCursor(p_painter, offset, cpos, m_cursorWidth);
        }

        offset.ry() += rect.height();
        if (block == lastBlock) {
            break;
        }

        block = block.next();
    }

    p_painter->setPen(oldPen);
}

QVector<QTextLayout::FormatRange> VTextDocumentLayout::formatRangeFromSelection(const QTextBlock &p_block,
                                                                                const QVector<Selection> &p_selections) const
{
    QVector<QTextLayout::FormatRange> ret;

    int blpos = p_block.position();
    int bllen = p_block.length();
    for (int i = 0; i < p_selections.size(); ++i) {
        const QAbstractTextDocumentLayout::Selection &range = p_selections.at(i);
        const int selStart = range.cursor.selectionStart() - blpos;
        const int selEnd = range.cursor.selectionEnd() - blpos;
        if (selStart < bllen
            && selEnd > 0
            && selEnd > selStart) {
            QTextLayout::FormatRange o;
            o.start = selStart;
            o.length = selEnd - selStart;
            o.format = range.format;
            ret.append(o);
        } else if (!range.cursor.hasSelection()
                   && range.format.hasProperty(QTextFormat::FullWidthSelection)
                   && p_block.contains(range.cursor.position())) {
            // For full width selections we don't require an actual selection, just
            // a position to specify the line. that's more convenience in usage.
            QTextLayout::FormatRange o;
            QTextLine l = p_block.layout()->lineForTextPosition(range.cursor.position() - blpos);
            o.start = l.textStart();
            o.length = l.textLength();
            if (o.start + o.length == bllen - 1) {
                ++o.length; // include newline
            }

            o.format = range.format;
            ret.append(o);
        }
    }

    return ret;
}

int VTextDocumentLayout::hitTest(const QPointF &p_point, Qt::HitTestAccuracy p_accuracy) const
{
    qDebug() << "VTextDocumentLayout hitTest()" << p_point;
    return -1;
}

int VTextDocumentLayout::pageCount() const
{
    return 1;
}

QSizeF VTextDocumentLayout::documentSize() const
{
    return QSizeF(m_width, m_height);
}

QRectF VTextDocumentLayout::frameBoundingRect(QTextFrame *p_frame) const
{
    Q_UNUSED(p_frame);
    return QRectF(0, 0, qMax(m_pageWidth, m_width), qreal(INT_MAX));
}

QRectF VTextDocumentLayout::blockBoundingRect(const QTextBlock &p_block) const
{
    if (!p_block.isValid()) {
        return QRectF();
    }

    const BlockInfo &info = m_blocks[p_block.blockNumber()];
    QRectF geo = info.m_rect.adjusted(0, info.m_offset, 0, info.m_offset);
    qDebug() << "blockBoundingRect()" << p_block.blockNumber() << p_block.text()
             << info.m_offset << info.m_rect << geo;
    Q_ASSERT(info.hasOffset());

    return geo;
}

void VTextDocumentLayout::documentChanged(int p_from, int p_charsRemoved, int p_charsAdded)
{
    QTextDocument *doc = document();
    int newBlockCount = doc->blockCount();

    // Update the margin.
    m_margin = doc->documentMargin();

    int charsChanged = p_charsRemoved + p_charsAdded;

    QTextBlock changeStartBlock = doc->findBlock(p_from);
    // May be an invalid block.
    QTextBlock changeEndBlock = doc->findBlock(qMax(0, p_from + charsChanged));

    bool needRelayout = false;
    if (changeStartBlock == changeEndBlock
        && newBlockCount == m_blockCount) {
        // Change single block internal only.
        QTextBlock block = changeStartBlock;
        if (block.isValid() && block.length()) {
            QRectF oldBr = blockBoundingRect(block);
            clearBlockLayout(block);
            layoutBlock(block);
            QRectF newBr = blockBoundingRect(block);
            // Only one block is affected.
            if (newBr.height() == oldBr.height()) {
                // Update document size.
                updateDocumentSizeWithOneBlockChanged(block.blockNumber());

                emit updateBlock(block);
                return;
            }
        }
    } else {
        // Clear layout of all affected blocks.
        QTextBlock block = changeStartBlock;
        do {
            clearBlockLayout(block);
            if (block == changeEndBlock) {
                break;
            }

            block = block.next();
        } while(block.isValid());

        needRelayout = true;
    }

    updateBlockCount(newBlockCount, changeStartBlock.blockNumber());

    if (needRelayout) {
        // Relayout all affected blocks.
        QTextBlock block = changeStartBlock;
        do {
            layoutBlock(block);
            if (block == changeEndBlock) {
                break;
            }

            block = block.next();
        } while(block.isValid());
    }

    updateDocumentSize();

    // TODO: Update the view of all the blocks after changeStartBlock.
    const BlockInfo &firstInfo = m_blocks[changeStartBlock.blockNumber()];
    emit update(QRectF(0., firstInfo.m_offset, 1000000000., 1000000000.));
}

void VTextDocumentLayout::clearBlockLayout(QTextBlock &p_block)
{
    p_block.clearLayout();
    int num = p_block.blockNumber();
    if (num < m_blocks.size()) {
        m_blocks[num].reset();
        clearOffsetFrom(num + 1);
    }
}

void VTextDocumentLayout::clearOffsetFrom(int p_blockNumber)
{
    for (int i = p_blockNumber; i < m_blocks.size(); ++i) {
        if (!m_blocks[i].hasOffset()) {
            Q_ASSERT(validateBlocks());
            break;
        }

        m_blocks[i].m_offset = -1;
    }
}

void VTextDocumentLayout::fillOffsetFrom(int p_blockNumber)
{
    qreal offset = m_blocks[p_blockNumber].bottom();
    for (int i = p_blockNumber + 1; i < m_blocks.size(); ++i) {
        BlockInfo &info = m_blocks[i];
        if (!info.m_rect.isNull()) {
            info.m_offset = offset;
            offset += info.m_rect.height();
        } else {
            break;
        }
    }
}

bool VTextDocumentLayout::validateBlocks() const
{
    bool valid = true;
    for (int i = 0; i < m_blocks.size(); ++i) {
        const BlockInfo &info = m_blocks[i];
        if (!info.hasOffset()) {
            valid = false;
        } else if (!valid) {
            return false;
        }
    }

    return true;
}

void VTextDocumentLayout::updateBlockCount(int p_count, int p_changeStartBlock)
{
    if (m_blockCount != p_count) {
        m_blockCount = p_count;
        m_blocks.resize(m_blockCount);

        // Fix m_blocks.
        QTextBlock block = document()->findBlockByNumber(p_changeStartBlock);
        while (block.isValid()) {
            BlockInfo &info = m_blocks[block.blockNumber()];
            info.reset();

            QRectF br = blockRectFromTextLayout(block);
            if (!br.isNull()) {
                info.m_rect = br;
            }

            block = block.next();
        }
    }
}

void VTextDocumentLayout::layoutBlock(const QTextBlock &p_block)
{
    QTextDocument *doc = document();

    // The height (y) of the next line.
    qreal height = 0;
    QTextLayout *tl = p_block.layout();
    QTextOption option = doc->defaultTextOption();
    tl->setTextOption(option);

    int extraMargin = 0;
    if (option.flags() & QTextOption::AddSpaceForLineAndParagraphSeparators) {
        QFontMetrics fm(p_block.charFormat().font());
        extraMargin += fm.width(QChar(0x21B5));
    }

    qreal availableWidth = m_pageWidth;
    if (availableWidth <= 0) {
        availableWidth = qreal(INT_MAX);
    }

    availableWidth -= 2 * m_margin + extraMargin;

    tl->beginLayout();

    while (true) {
        QTextLine line = tl->createLine();
        if (!line.isValid()) {
            break;
        }

        line.setLeadingIncluded(true);
        line.setLineWidth(availableWidth);
        line.setPosition(QPointF(m_margin, height));
        height += line.height();
    }

    tl->endLayout();

    // Set this block's line count to its layout's line count.
    // That is one block may occupy multiple visual lines.
    const_cast<QTextBlock&>(p_block).setLineCount(p_block.isVisible() ? tl->lineCount() : 0);

    // Update the info about this block.
    finishBlockLayout(p_block);
}

void VTextDocumentLayout::finishBlockLayout(const QTextBlock &p_block)
{
    // Update rect and offset.
    Q_ASSERT(p_block.isValid());
    int num = p_block.blockNumber();
    Q_ASSERT(m_blocks.size() > num);
    BlockInfo &info = m_blocks[num];
    info.reset();
    info.m_rect = blockRectFromTextLayout(p_block);
    Q_ASSERT(!info.m_rect.isNull());
    int pre = previousValidBlockNumber(num);
    if (pre == -1) {
        info.m_offset = 0;
    } else if (m_blocks[pre].hasOffset()) {
        info.m_offset = m_blocks[pre].bottom();
    }

    if (info.hasOffset()) {
        fillOffsetFrom(num);
    }
}

int VTextDocumentLayout::previousValidBlockNumber(int p_number) const
{
    return p_number >= 0 ? p_number - 1 : -1;
}

void VTextDocumentLayout::updateDocumentSize()
{
    // The last valid block.
    int idx = previousValidBlockNumber(m_blocks.size());
    Q_ASSERT(idx > -1);
    if (m_blocks[idx].hasOffset()) {
        int oldHeight = m_height;
        int oldWidth = m_width;

        m_height = m_blocks[idx].bottom();

        m_width = 0;
        for (int i = 0; i < m_blocks.size(); ++i) {
            const BlockInfo &info = m_blocks[i];
            Q_ASSERT(info.hasOffset());
            if (m_width < info.m_rect.width()) {
                m_width = info.m_rect.width();
                m_maximumWidthBlockNumber = i;
            }
        }

        // Allow the cursor to be displayed.
        m_width = blockWidthInDocument(m_width);

        if (oldHeight != m_height
            || oldWidth != m_width) {
            emit documentSizeChanged(documentSize());
        }
    }
}

void VTextDocumentLayout::setCursorWidth(int p_width)
{
    m_cursorWidth = p_width;
}

int VTextDocumentLayout::cursorWidth() const
{
    return m_cursorWidth;
}

QRectF VTextDocumentLayout::blockRectFromTextLayout(const QTextBlock &p_block)
{
    QTextLayout *tl = p_block.layout();
    if (tl->lineCount() < 1) {
        return QRectF();
    }

    QRectF br(QPointF(0, 0), tl->boundingRect().bottomRight());

    // Not know why. Copied from QPlainTextDocumentLayout.
    if (tl->lineCount() == 1) {
        br.setWidth(qMax(br.width(), tl->lineAt(0).naturalTextWidth()));
    }

    br.adjust(0, 0, m_margin, 0);
    if (!p_block.next().isValid()) {
        br.adjust(0, 0, 0, m_margin);
    }

    return br;
}

void VTextDocumentLayout::updateDocumentSizeWithOneBlockChanged(int p_blockNumber)
{
    const BlockInfo &info = m_blocks[p_blockNumber];
    qreal width = blockWidthInDocument(info.m_rect.width());
    if (width > m_width) {
        m_width = width;
        m_maximumWidthBlockNumber = p_blockNumber;
        emit documentSizeChanged(documentSize());
    } else if (width < m_width && p_blockNumber == m_maximumWidthBlockNumber) {
        // Shrink the longest block.
        updateDocumentSize();
    }
}
