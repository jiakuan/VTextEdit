#ifndef VTEXTDOCUMENTLAYOUT_H
#define VTEXTDOCUMENTLAYOUT_H

#include <QAbstractTextDocumentLayout>
#include <QVector>
#include <QSize>

class VImageResourceManager2;
struct VBlockImageInfo2;


class VTextDocumentLayout : public QAbstractTextDocumentLayout
{
    Q_OBJECT
public:
    VTextDocumentLayout(QTextDocument *p_doc,
                        VImageResourceManager2 *p_imageMgr);

    void draw(QPainter *p_painter, const PaintContext &p_context) Q_DECL_OVERRIDE;

    int hitTest(const QPointF &p_point, Qt::HitTestAccuracy p_accuracy) const Q_DECL_OVERRIDE;

    int pageCount() const Q_DECL_OVERRIDE;

    QSizeF documentSize() const Q_DECL_OVERRIDE;

    QRectF frameBoundingRect(QTextFrame *p_frame) const Q_DECL_OVERRIDE;

    QRectF blockBoundingRect(const QTextBlock &p_block) const Q_DECL_OVERRIDE;

    void setCursorWidth(int p_width);

    int cursorWidth() const;

    void setLineLeading(qreal p_leading);

    qreal getLineLeading() const;

    // Return the block number which contains point @p_point.
    // If @p_point is at the border, returns the block below.
    int findBlockByPosition(const QPointF &p_point) const;

    void setImageWidthConstrainted(bool p_enabled);

    void setBlockImageEnabled(bool p_enabled);

protected:
    void documentChanged(int p_from, int p_charsRemoved, int p_charsAdded) Q_DECL_OVERRIDE;

private:
    struct BlockInfo
    {
        BlockInfo()
        {
            reset();
        }

        void reset()
        {
            m_offset = -1;
            m_rect = QRectF();
        }

        bool hasOffset() const
        {
            return m_offset > -1 && !m_rect.isNull();
        }

        qreal top() const
        {
            Q_ASSERT(hasOffset());
            return m_offset;
        }

        qreal bottom() const
        {
            Q_ASSERT(hasOffset());
            return m_offset + m_rect.height();
        }

        // Y offset of this block.
        // -1 for invalid.
        qreal m_offset;

        // The bounding rect of this block, including the margins.
        // Null for invalid.
        QRectF m_rect;
    };

    void layoutBlock(const QTextBlock &p_block);

    // Clear the layout of @p_block.
    // Also clear all the offset behind this block.
    void clearBlockLayout(QTextBlock &p_block);

    // Clear the offset of all the blocks from @p_blockNumber.
    void clearOffsetFrom(int p_blockNumber);

    // Fill the offset filed from @p_blockNumber + 1.
    void fillOffsetFrom(int p_blockNumber);

    // Update block count to @p_count due to document change.
    // Maintain m_blocks.
    // @p_changeStartBlock is the block number of the start block in this change.
    void updateBlockCount(int p_count, int p_changeStartBlock);

    bool validateBlocks() const;

    void finishBlockLayout(const QTextBlock &p_block);

    int previousValidBlockNumber(int p_number) const;

    int nextValidBlockNumber(int p_number) const;

    // Update block count and m_blocks size.
    void updateDocumentSize();

    QVector<QTextLayout::FormatRange> formatRangeFromSelection(const QTextBlock &p_block,
                                                               const QVector<Selection> &p_selections) const;

    // Get the block range [first, last] by rect @p_rect.
    // @p_rect: a clip region in document coordinates. If null, returns all the blocks.
    // Return [-1, -1] if no valid block range found.
    void blockRangeFromRect(const QRectF &p_rect, int &p_first, int &p_last) const;

    // Binary search to get the block range [first, last] by @p_rect.
    void blockRangeFromRectBS(const QRectF &p_rect, int &p_first, int &p_last) const;

    // Return a rect from the layout.
    // Return a null rect if @p_block has not been layouted.
    QRectF blockRectFromTextLayout(const QTextBlock &p_block);

    // Update document size when only block @p_blockNumber is changed and the height
    // remain the same.
    void updateDocumentSizeWithOneBlockChanged(int p_blockNumber);

    void adjustImagePaddingAndSize(const VBlockImageInfo2 *p_info,
                                   int p_maximumWidth,
                                   int &p_padding,
                                   QSize &p_size) const;

    // Draw images of block @p_block.
    // @p_offset: the offset for the drawing of the block.
    void drawBlockImage(QPainter *p_painter,
                        const QTextBlock &p_block,
                        const QPointF &p_offset);

    // Document margin on left/right/bottom.
    qreal m_margin;

    // Maximum width of the contents.
    qreal m_width;

    // The block number of the block which contains the m_width.
    int m_maximumWidthBlockNumber;

    // Height of all the document (all the blocks).
    qreal m_height;

    // Set the leading space of a line.
    qreal m_lineLeading;

    // Block count of the document.
    int m_blockCount;

    // Width of the cursor.
    int m_cursorWidth;

    // Right margin for cursor.
    qreal m_cursorMargin;

    QVector<BlockInfo> m_blocks;

    VImageResourceManager2 *m_imageMgr;

    bool m_blockImageEnabled;

    // Whether constraint the width of image to the width of the page.
    bool m_imageWidthConstrainted;
};

inline qreal VTextDocumentLayout::getLineLeading() const
{
    return m_lineLeading;
}

#endif // VTEXTDOCUMENTLAYOUT_H
