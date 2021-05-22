#include <QDesktopWidget>
#include <QScreen>
#include <QApplication>
#include <QKeyEvent>
#include <QShortcut>
#include <QtMath>
#include <QDebug>
#include <QGraphicsView>
#include <QVBoxLayout>
#include "MainWindow.h"
#include "scene.h"

//#define DRAG_ENABLED

MainWindow::MainWindow(QWidget* parent) :
    QMainWindow(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setWindowFlags(Qt::WindowStaysOnTopHint |
                   Qt::WindowTitleHint |
                   Qt::WindowCloseButtonHint |
                   Qt::WindowSystemMenuHint);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
#ifdef Q_OS_WIN
    setWindowOpacity(0.1);
#endif

    initialize();
}

void MainWindow::initialize()
{
    m_renderData.scale = kMinScale;
    m_renderData.isActivated = false;
    m_renderData.isMeasurerMode = false;
    m_renderData.centerShiftX = 0;
    m_renderData.centerShiftY = 0;
    m_renderData.scaleShiftX = 0;
    m_renderData.scaleShiftY = 0;
    m_renderData.fixedRectangle = QRect(0, 0, 0, 0);
    m_renderData.measureRectangle = QRect(0, 0, 0, 0);

    auto shortcut = new QShortcut(QKeySequence(Qt::Key_P), this);
    connect(shortcut, &QShortcut::activated, this, [&](){
        switchPalette();
        update();
    });

    m_scene = new Scene(this);
    m_scene->setPalette(m_palettes[m_paletteIndex]);

    m_view = new View(this);
    m_view->setScene(m_scene);
    m_view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto layout = new QVBoxLayout();
    layout->addWidget(m_view);
    layout->setMargin(0);

    auto centralWidget = new QWidget(this);
    centralWidget->setLayout(layout);
    setCentralWidget(centralWidget);

    connect(m_view, &View::mouseMoved, this, &MainWindow::onMouseMove);
    connect(m_view, &View::mousePressed, this, &MainWindow::onMousePress);
    connect(m_view, &View::mouseReleaseed, this, &MainWindow::onMouseRelease);
}

void MainWindow::enterEvent(QEvent* event)
{
    static bool isFirstEnter{true};
    grabScreen();
    m_view->show();
#ifdef Q_OS_WIN
    setWindowOpacity(1);
#endif
    m_renderData.isActivated = true;

    if (!isFirstEnter)
    {
        adjust(m_lastWindowPos.x() - pos().x(),
               m_lastWindowPos.y() - pos().y());
    }

    isFirstEnter = false;

    m_scene->setRenderData(m_renderData);

    event->accept();
}

void MainWindow::leaveEvent(QEvent* event)
{
#ifdef Q_OS_WIN
    setWindowOpacity(0.1);
#endif
    m_view->hide();
    m_renderData.isActivated = false;
    m_lastWindowPos = pos();

    calculateShifts();

    m_scene->setRenderData(m_renderData);

    event->accept();
}

void MainWindow::onMouseMove(QMouseEvent* event)
{
    auto x = calculateScaledX(event->x());
    auto y = calculateScaledY(event->y());

    m_renderData.cursorPoint = {x, y};

    if (m_renderData.isMeasurerMode)
    {
        m_renderData.measureRectangle.setBottomRight({x, y});
    }

#ifdef DRAG_ENABLED
    if (event->buttons() & Qt::RightButton)
    {
        auto shiftX = m_lastMousePos.x() - event->x() / m_renderData.scale;
        auto shiftY = m_lastMousePos.y() - event->y() / m_renderData.scale;

        if (abs(shiftX) < m_renderData.scaleShiftX)
        {
            m_renderData.centerShiftX = shiftX;
        }

        if (abs(shiftY) < m_renderData.scaleShiftY)
        {
            m_renderData.centerShiftY = shiftY;
        }
    }
#endif

    calculate();

    m_scene->setRenderData(m_renderData);

    event->accept();
}

void MainWindow::wheelEvent(QWheelEvent* event)
{
    QPoint numPixels = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;

    if (!numPixels.isNull())
    {
        changeScale(numPixels);
    }
    else if (!numDegrees.isNull())
    {
        changeScale(numDegrees);
    }

    auto x = calculateScaledX(event->position().x());
    auto y = calculateScaledY(event->position().y());

    m_renderData.cursorPoint = {x, y};

    calculate();

    m_scene->setRenderData(m_renderData);

    event->accept();
}

void MainWindow::onMousePress(QMouseEvent* event)
{
    auto x = calculateScaledX(event->x());
    auto y = calculateScaledY(event->y());

    if (event->button() == Qt::LeftButton)
    {
        if (m_renderData.fixedRectangle != m_renderData.cursorRectangle)
        {
            setFixedRectangle();
        }
        else
        {
            clearFixedRectangle();
        }
    }
    else
    {
        m_renderData.isMeasurerMode = true;
        m_renderData.measureRectangle.setTopLeft({x, y});
        m_renderData.measureRectangle.setBottomRight({x, y});
    }

    m_lastMousePos = {m_renderData.centerShiftX + event->x() / m_renderData.scale,
                      m_renderData.centerShiftY + event->y() / m_renderData.scale};

    calculate();

    m_scene->setRenderData(m_renderData);
    //update();

    event->accept();
}

void MainWindow::onMouseRelease(QMouseEvent* event)
{
    m_renderData.isMeasurerMode = false;

    m_scene->setRenderData(m_renderData);

    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent*)
{
    calculateShifts();
    m_renderData.windowRectangle = rect();
}

void MainWindow::grabScreen()
{
    auto screen = QGuiApplication::primaryScreen();
    auto descktop = QApplication::desktop();
    m_renderData.screenImage = screen->grabWindow(descktop->winId()).copy(geometry().adjusted(1, 1, -1, -1));
}

void MainWindow::setFixedRectangle()
{
    m_renderData.fixedRectangle = m_renderData.cursorRectangle;
}

void MainWindow::clearFixedRectangle()
{
    m_renderData.fixedRectangle = {0, 0, 0, 0};
}

void MainWindow::calculate()
{
    auto x = m_renderData.cursorPoint.x();
    auto y = m_renderData.cursorPoint.y();
    auto w = m_renderData.screenImage.width();
    auto h = m_renderData.screenImage.height();
    auto color = m_renderData.screenImage.toImage().pixel(x, y);

    auto cr = beamTo(x, w - 1, y, 1, Qt::Horizontal, color);
    auto cl = beamTo(x, 0, y, -1, Qt::Horizontal, color);
    auto cb = beamTo(y, h - 1, x, 1, Qt::Vertical, color);
    auto ct = beamTo(y, 0, x, -1, Qt::Vertical, color);

    int fl, ft, fb, fr;
    int wl, wt, wb, wr;
    m_renderData.fixedRectangle.getCoords(&fl, &ft, &fr, &fb);
    m_renderData.windowRectangle.getCoords(&wl, &wt, &wr, &wb);
    m_renderData.scaledRectangle = {m_renderData.scaleShiftX, m_renderData.scaleShiftY,
                                   rect().width() / m_renderData.scale - 1, rect().height() / m_renderData.scale - 1};


    m_renderData.fixedLines[0] = {wl, ft, wr, ft};
    m_renderData.fixedLines[1] = {wl, fb + 1, wr, fb + 1};
    m_renderData.fixedLines[2] = {fl, wb, fl, wt};
    m_renderData.fixedLines[3] = {fr + 1, wb, fr + 1, wt};

    m_renderData.cursorHLine = {cl, y, cr, y};
    m_renderData.cursorVLine = {x, ct, x, cb};
    m_renderData.cursorRectangle = {cl, ct, cr - cl, cb - ct};

    auto ccx = m_renderData.cursorRectangle.center().x();
    auto ccy = m_renderData.cursorRectangle.center().y();
    auto fcx = m_renderData.fixedRectangle.center().x();
    auto fcy = m_renderData.fixedRectangle.center().y();
    int hx1{0},hx2{0},hy1{0},hy2{0};
    int vx1{0},vx2{0},vy1{0},vy2{0};

    if (m_renderData.cursorRectangle == m_renderData.fixedRectangle)
    {
    }
    else
    {
        if (m_renderData.cursorRectangle.contains(m_renderData.fixedRectangle))
        {
            hx2 = fl - 1;   hy2 = fcy;      hx1 = cl;   hy1 = fcy;
            vx2 = fcx;      vy2= ft - 1;    vx1 = fcx;  vy1 = ct;
        }
        else if (m_renderData.fixedRectangle.contains(m_renderData.cursorRectangle))
        {
            hx2 = cl - 1;   hy2 = ccy;      hx1 =fl;    hy1 = ccy;
            vx2 = ccx;      vy2 = ct - 1;   vx1 = ccx;  vy1 = ft;
        }
        else
        {
            vx1 = vx2 = ccx;
            hy1 = hy2 = ccy;

            if (cb < ft)
            {
                vy1 = cb + 1; vy2 = ft - 1;
            }
            else if (ct > fb)
            {
                vy1 = fb + 2; vy2 = ct - 1;
            }
            else
            {
                if (ct < ft)
                {
                    vy1 = ct; vy2 = ft - 1;
                }
                else if (ct > ft)
                {
                     vy1 = ft; vy2 = ct - 1;
                }
            }

            if (cr < fl)
            {
                hx1 = cr + 1; hx2 = fl - 1;
            }
            else if (cl > fr)
            {
                hx1 = fr + 2; hx2 = cl - 1;
            }
            else
            {
                if (cl < fl)
                {
                    hx1 = cl;  hx2 = fl - 1;
                }
                else if (cl > fl)
                {
                    hx1 = fl; hx2 = cl - 1;
                }
            }
        }
    }

    m_renderData.measureVLine = {vx1, vy1, vx2, vy2};
    m_renderData.measureHLine = {hx1, hy1, hx2, hy2};
}

int MainWindow::calculateScaledX(int x)
{
    return m_renderData.scaleShiftX + x / m_renderData.scale + m_renderData.centerShiftX;
}

int MainWindow::calculateScaledY(int y)
{
    return m_renderData.scaleShiftY + y / m_renderData.scale + m_renderData.centerShiftY;
}

int MainWindow::beamTo(int startPos, int endPos, int coord, int step,
                       Qt::Orientation orientation, const QRgb& color)
{
    int resPos = endPos;
    for (int pos = startPos + step; pos != endPos; pos += step)
    {
        auto point = orientation == Qt::Horizontal ? QPoint(pos, coord)
                                                   : QPoint(coord, pos);

        if (!m_renderData.screenImage.rect().contains(point))
        {
            break;
        }

        if (m_renderData.screenImage.toImage().pixel(point) != color)
        {
            return pos - step;
        }
    }
    return resPos;
}

void MainWindow::calculateShifts()
{
    auto w = rect().width() / 2;
    auto h = rect().height() / 2;
    m_renderData.scaleShiftX = w - w / m_renderData.scale;
    m_renderData.scaleShiftY = h - h / m_renderData.scale;
}

void MainWindow::changeScale(const QPoint& delta)
{
    m_renderData.scale += delta.y() > 0 ? 1 : -1;

    if (m_renderData.scale > kMaxScale)
    {
        m_renderData.scale = kMaxScale;
    }

    if (m_renderData.scale < kMinScale)
    {
        m_renderData.scale = kMinScale;
    }

    calculateShifts();
    m_view->resetTransform();
    m_view->scale(m_renderData.scale, m_renderData.scale);
}

void MainWindow::switchPalette()
{
    if(++m_paletteIndex >= m_palettes.size())
    {
        m_paletteIndex = 0;
    }
    m_scene->setPalette(m_palettes[m_paletteIndex]);
}

void MainWindow::adjust(int dx, int dy)
{
    m_renderData.fixedRectangle.translate(dx, dy);
}

