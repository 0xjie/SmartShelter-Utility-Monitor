// Page-building functions for MainWindow
#include "mainwindow.h"
#include "mainwindow_utils.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpinBox>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>

#include "databasemanager.h"
#include "dbconfig.h"

void MainWindow::buildMainPages() {
    buildRealtimePage();
    m_builtPages.insert(0);
}

void MainWindow::buildRealtimePage() {
    auto* rootLayout = ui->verticalLayoutDashboard;
    clearLayout(rootLayout);
    rootLayout->setSpacing(14);

    auto* headerCard = createPanelCard(ui->pageDashboard);
    auto* headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 16, 20, 16);

    auto* titleWrap = new QVBoxLayout();
    auto* titleLabel = new QLabel(QStringLiteral("实时监控"), headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    auto* subLabel = new QLabel(QStringLiteral("集中查看关键指标实时数据与曲线。"), headerCard);
    subLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;}");
    titleWrap->addWidget(titleLabel);
    titleWrap->addWidget(subLabel);

    auto* rightWrap = new QVBoxLayout();
    m_realtimeStatusLabel = new QLabel(QStringLiteral("系统运行状态：正常"), headerCard);
    m_realtimeStatusLabel->setStyleSheet("QLabel{color:#059669;font-size:14px;font-weight:700;}");
    rightWrap->addWidget(m_realtimeStatusLabel, 0, Qt::AlignRight);

    headerLayout->addLayout(titleWrap);
    headerLayout->addStretch();
    headerLayout->addLayout(rightWrap);
    rootLayout->addWidget(headerCard);

    auto createCard = [&](const QString& title, QLabel*& valueLabel, QLabel*& dotLabel, QLabel*& stateLabel) -> QFrame* {
        auto* card = new QFrame(ui->pageDashboard);
        card->setObjectName("statusCard");
        applyShadow(card);
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 14, 16, 14);
        layout->setSpacing(8);

        auto* titleLabelLocal = new QLabel(title, card);
        titleLabelLocal->setStyleSheet("QLabel{color:#9cc7ff;font-size:13px;font-weight:600;}");
        valueLabel = new QLabel(QStringLiteral("--"), card);
        valueLabel->setStyleSheet("QLabel{color:#f8fbff;font-size:28px;font-weight:800;}");
        dotLabel = new QLabel(card);
        dotLabel->setFixedSize(12, 12);
        dotLabel->setStyleSheet("QLabel{background:#22c55e;border-radius:6px;}");
        stateLabel = new QLabel(QStringLiteral("正常"), card);
        stateLabel->setStyleSheet("QLabel{color:#22c55e;font-size:12px;font-weight:600;}");
        auto* stateRow = new QHBoxLayout();
        stateRow->addWidget(dotLabel);
        stateRow->addWidget(stateLabel);
        stateRow->addStretch();

        layout->addWidget(titleLabelLocal);
        layout->addWidget(valueLabel);
        layout->addLayout(stateRow);
        return card;
    };

    auto* grid = new QGridLayout();
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(12);
    grid->addWidget(createCard(QStringLiteral("温度"), m_cardTempValue, m_dotTemp, m_stateTempLabel), 0, 0);
    grid->addWidget(createCard(QStringLiteral("湿度"), m_cardHumiValue, m_dotHumi, m_stateHumiLabel), 0, 1);
    grid->addWidget(createCard(QStringLiteral("水流"), m_cardFlowValue, m_dotFlow, m_stateFlowLabel), 0, 2);
    grid->addWidget(createCard(QStringLiteral("PM2.5"), m_cardPmValue, m_dotPm, m_statePmLabel), 1, 0);
    grid->addWidget(createCard(QStringLiteral("空气指数"), m_cardAirValue, m_dotAir, m_stateAirLabel), 1, 1);
    grid->addWidget(createCard(QStringLiteral("电流"), m_cardCurrentValue, m_dotCurrent, m_stateCurrentLabel), 1, 2);
    rootLayout->addLayout(grid);

    m_currentSeries = new QLineSeries(this);
    m_currentSeries->setName(QStringLiteral("电流 (A)"));
    m_flowSeries = new QLineSeries(this);
    m_flowSeries->setName(QStringLiteral("水流 (L/min)"));

    auto* chart = new QChart();
    chart->setBackgroundVisible(false);
    chart->setPlotAreaBackgroundVisible(true);
    chart->setPlotAreaBackgroundBrush(QColor(8, 27, 58, 210));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignTop);
    chart->legend()->setLabelColor(QColor(0xdb, 0xea, 0xfe));
    chart->addSeries(m_currentSeries);
    chart->addSeries(m_flowSeries);
    m_currentSeries->setColor(QColor(251, 191, 36));   // 金色：电流
    m_flowSeries->setColor(QColor(56, 189, 248));      // 蓝色：水流

    m_axisX = new QDateTimeAxis(this);
    m_axisX->setFormat("HH:mm:ss");
    m_axisX->setLabelsColor(QColor(0xdb, 0xea, 0xfe));
    m_axisX->setGridLineColor(QColor(125, 211, 252, 35));
    chart->addAxis(m_axisX, Qt::AlignBottom);
    m_currentSeries->attachAxis(m_axisX);
    m_flowSeries->attachAxis(m_axisX);

    // 左侧Y轴：电流 (A)
    m_axisY_Current = new QValueAxis(this);
    m_axisY_Current->setTitleText(QStringLiteral("电流 (A)"));
    m_axisY_Current->setTitleBrush(QColor(251, 191, 36));
    m_axisY_Current->setRange(0, 20);
    m_axisY_Current->setLabelsColor(QColor(251, 191, 36));
    m_axisY_Current->setGridLineColor(QColor(125, 211, 252, 35));
    chart->addAxis(m_axisY_Current, Qt::AlignLeft);
    m_currentSeries->attachAxis(m_axisY_Current);

    // 右侧Y轴：水流 (L/min)
    m_axisY_Flow = new QValueAxis(this);
    m_axisY_Flow->setTitleText(QStringLiteral("水流 (L/min)"));
    m_axisY_Flow->setTitleBrush(QColor(56, 189, 248));
    m_axisY_Flow->setRange(0, 10);
    m_axisY_Flow->setLabelsColor(QColor(56, 189, 248));
    m_axisY_Flow->setGridLineColor(QColor(125, 211, 252, 35));
    chart->addAxis(m_axisY_Flow, Qt::AlignRight);
    m_flowSeries->attachAxis(m_axisY_Flow);

    auto* chartCard = createPanelCard(ui->pageDashboard);
    auto* chartLayout = new QVBoxLayout(chartCard);
    chartLayout->setContentsMargins(12, 12, 12, 12);
    auto* chartView = new QChartView(chart, chartCard);
    chartView->setRenderHint(QPainter::Antialiasing, true);
    chartView->setStyleSheet("background:transparent;border:none;");
    chartLayout->addWidget(chartView);
    rootLayout->addWidget(chartCard, 1);
}

void MainWindow::buildWaterPowerPage() {
    auto* rootLayout = ui->verticalLayoutRemote;
    clearLayout(rootLayout);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    m_wpAnalysisDateEdit = nullptr;
    m_wpTrendDaysCombo = nullptr;
    m_wpUsagePowerSet = nullptr;
    m_wpUsageWaterSet = nullptr;
    m_wpUsagePowerSeries = nullptr;
    m_wpUsageWaterSeries = nullptr;
    m_wpUsagePowerChart = nullptr;
    m_wpUsageWaterChart = nullptr;
    m_wpUsagePowerChartView = nullptr;
    m_wpUsageWaterChartView = nullptr;
    m_wpUsageAxisX_Power = nullptr;
    m_wpUsageAxisX_Water = nullptr;
    m_wpUsageAxisY_Power = nullptr;
    m_wpUsageAxisY_Water = nullptr;
    m_wpUsageStatsLabel = nullptr;
    m_wpTrendPowerSet = nullptr;
    m_wpTrendWaterSet = nullptr;
    m_wpTrendPowerSeries = nullptr;
    m_wpTrendWaterSeries = nullptr;
    m_wpTrendPowerChart = nullptr;
    m_wpTrendWaterChart = nullptr;
    m_wpTrendPowerChartView = nullptr;
    m_wpTrendWaterChartView = nullptr;
    m_wpTrendAxisX_Power = nullptr;
    m_wpTrendAxisX_Water = nullptr;
    m_wpTrendAxisY_Power = nullptr;
    m_wpTrendAxisY_Water = nullptr;
    m_wpTrendStatsLabel = nullptr;
    m_wpLoadValueLabel = nullptr;
    m_wpLoadStatusLabel = nullptr;
    m_wpTodayPowerLabel = nullptr;
    m_wpTodayWaterLabel = nullptr;
    m_wpRecentRangeLabel = nullptr;
    m_wpRecentPowerLabel = nullptr;
    m_wpRecentWaterLabel = nullptr;

    auto* scrollArea = new QScrollArea(ui->pageRemote);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        "QScrollArea{background:transparent;border:none;}"
        "QScrollArea > QWidget > QWidget{background:transparent;}");

    auto* contentWidget = new QWidget(scrollArea);
    auto* pageLayout = new QVBoxLayout(contentWidget);
    pageLayout->setContentsMargins(14, 14, 14, 18);
    pageLayout->setSpacing(14);

    {
        auto* headerCard = createPanelCard(contentWidget);
        auto* headerLayout = new QHBoxLayout(headerCard);
        headerLayout->setContentsMargins(20, 16, 20, 16);
        auto* titleWrap = new QVBoxLayout();
        auto* titleLabel = new QLabel(QStringLiteral("水电分析"), headerCard);
        titleLabel->setStyleSheet("QLabel{color:#f8fbff;font-size:20px;font-weight:800;}");
        auto* subtitleLabel = new QLabel(QStringLiteral("查看某一天里不同时段的水电消耗，以及过去多天的变化趋势。"), headerCard);
        subtitleLabel->setWordWrap(true);
        subtitleLabel->setStyleSheet("QLabel{color:#94a3b8;font-size:12px;margin-top:2px;}");
        titleWrap->addWidget(titleLabel);
        titleWrap->addWidget(subtitleLabel);
        headerLayout->addLayout(titleWrap);
        headerLayout->addStretch();
        auto* exportBtn = new QPushButton(QStringLiteral("导出当前分析"), headerCard);
        exportBtn->setStyleSheet(
            "QPushButton{background:#0f5fa8;color:white;border:1px solid #2f7fca;border-radius:8px;padding:8px 18px;font-size:13px;font-weight:700;}"
            "QPushButton:hover{background:#1a7ad4;}");
        connect(exportBtn, &QPushButton::clicked, this, [this]() {
            exportWaterPowerAnalysis(true, true);
        });
        headerLayout->addWidget(exportBtn);
        pageLayout->addWidget(headerCard);
    }

    {
        auto* resCard = createPanelCard(contentWidget);
        auto* resLayout = new QVBoxLayout(resCard);
        resLayout->setContentsMargins(24, 16, 24, 16);
        resLayout->setSpacing(14);
        auto* resTitle = new QLabel(QStringLiteral("水电剩余资源"), resCard);
        resTitle->setStyleSheet("QLabel{color:#7dd3fc;font-size:15px;font-weight:700;}");
        resLayout->addWidget(resTitle);

        auto* mainRow = new QHBoxLayout();
        mainRow->setSpacing(18);

        auto makeResourcePane = [&](const QString& titleText,
                                    const QString& dotColor,
                                    QWidget*& gaugeWidget,
                                    QLabel*& infoLabel,
                                    bool battery) {
            auto* pane = new QWidget(resCard);
            auto* paneLayout = new QHBoxLayout(pane);
            paneLayout->setContentsMargins(0, 0, 0, 0);
            paneLayout->setSpacing(14);

            auto* gaugeCol = new QVBoxLayout();
            auto* dot = new QLabel(pane);
            dot->setFixedSize(10, 10);
            dot->setStyleSheet(QString("QLabel{background:%1;border-radius:5px;}").arg(dotColor));
            auto* label = new QLabel(titleText, pane);
            label->setStyleSheet("QLabel{color:#bfdcff;font-size:12px;font-weight:700;}");
            auto* head = new QHBoxLayout();
            head->addWidget(dot);
            head->addWidget(label);
            head->addStretch();
            gaugeCol->addLayout(head);

            gaugeWidget = battery ? static_cast<QWidget*>(new BatteryGauge(pane))
                                  : static_cast<QWidget*>(new TankGauge(pane));
            gaugeWidget->setMinimumSize(88, 132);
            gaugeCol->addWidget(gaugeWidget, 0, Qt::AlignCenter);

            infoLabel = new QLabel(QStringLiteral("等待数据..."), pane);
            infoLabel->setTextFormat(Qt::RichText);
            infoLabel->setWordWrap(true);
            infoLabel->setMinimumWidth(260);
            infoLabel->setStyleSheet("QLabel{color:#cbd5e1;font-size:12px;}");

            paneLayout->addLayout(gaugeCol, 0);
            paneLayout->addWidget(infoLabel, 1);
            return pane;
        };

        mainRow->addWidget(makeResourcePane(QStringLiteral("电力"), QStringLiteral("#22c55e"), m_batteryGauge, m_batteryInfoLabel, true), 1);
        auto* sep = new QFrame(resCard);
        sep->setFrameShape(QFrame::VLine);
        sep->setStyleSheet("QFrame{color:rgba(255,255,255,0.08);}");
        mainRow->addWidget(sep);
        mainRow->addWidget(makeResourcePane(QStringLiteral("供水"), QStringLiteral("#3b82f6"), m_tankGauge, m_tankInfoLabel, false), 1);

        resLayout->addLayout(mainRow);
        pageLayout->addWidget(resCard);
    }

    auto* analysisRow = new QHBoxLayout();
    analysisRow->setSpacing(14);
    const QString analysisControlStyle =
        "QDateEdit,QComboBox{background:rgba(7,26,54,0.88);color:#e8f1ff;border:1px solid rgba(96,165,250,0.45);border-radius:8px;padding:6px 10px;font-size:13px;}"
        "QDateEdit:hover,QComboBox:hover{border-color:rgba(96,165,250,0.7);}";

    {
        auto* usageCard = createPanelCard(contentWidget);
        auto* usageLayout = new QVBoxLayout(usageCard);
        usageLayout->setContentsMargins(18, 14, 18, 14);
        usageLayout->setSpacing(10);
        auto* usageTitle = new QLabel(QStringLiteral("单日用量分析"), usageCard);
        usageTitle->setStyleSheet("QLabel{color:#7dd3fc;font-size:15px;font-weight:700;}");
        usageLayout->addWidget(usageTitle);

        auto* queryLayout = new QHBoxLayout();
        queryLayout->setSpacing(10);
        auto* dateLabel = new QLabel(QStringLiteral("日期"), usageCard);
        dateLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
        m_wpAnalysisDateEdit = new QDateEdit(QDate::currentDate().addDays(-1), usageCard);
        m_wpAnalysisDateEdit->setCalendarPopup(true);
        m_wpAnalysisDateEdit->setMaximumDate(QDate::currentDate());
        m_wpAnalysisDateEdit->setDisplayFormat("yyyy-MM-dd");
        m_wpAnalysisDateEdit->setStyleSheet(analysisControlStyle);
        queryLayout->addWidget(dateLabel);
        queryLayout->addWidget(m_wpAnalysisDateEdit);
        queryLayout->addStretch();
        usageLayout->addLayout(queryLayout);

        m_wpUsageStatsLabel = new QLabel(QStringLiteral("--"), usageCard);
        m_wpUsageStatsLabel->setWordWrap(true);
        m_wpUsageStatsLabel->setStyleSheet("QLabel{color:#e8f1ff;font-size:13px;line-height:1.9;padding:4px 0 10px 0;}");
        usageLayout->addWidget(m_wpUsageStatsLabel);

        // 图表横向滚动区域
        auto* chartScroll = new QScrollArea(usageCard);
        chartScroll->setWidgetResizable(true);
        chartScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        chartScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        chartScroll->setFixedHeight(550);
        chartScroll->setStyleSheet(
            "QScrollArea{background:transparent;border:none;}"
            "QScrollArea>QWidget>QWidget{background:transparent;}"
            "QScrollBar:horizontal{background:#081b3a;height:8px;border-radius:4px;}"
            "QScrollBar::handle:horizontal{background:#1e4a7a;border-radius:4px;min-width:50px;}"
            "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0px;border:none;}"
            "QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{background:transparent;}");

        auto* chartContainer = new QWidget(chartScroll);
        auto* chartCLayout = new QVBoxLayout(chartContainer);
        chartCLayout->setContentsMargins(0, 0, 0, 0);
        chartCLayout->setSpacing(8);

        auto* powerChartTitle = new QLabel(QStringLiteral("分时用电"), chartContainer);
        powerChartTitle->setStyleSheet("QLabel{color:#fbbf24;font-size:13px;font-weight:700;padding-top:4px;}");
        chartCLayout->addWidget(powerChartTitle);

        m_wpUsagePowerSet = new QBarSet(QStringLiteral("用电(Ah)"));
        m_wpUsagePowerSet->setColor(QColor(245, 158, 11));
        m_wpUsagePowerSeries = new QBarSeries(this);
        m_wpUsagePowerSeries->append(m_wpUsagePowerSet);
        m_wpUsagePowerSeries->setLabelsVisible(true);
        m_wpUsagePowerSeries->setLabelsFormat(QStringLiteral("@value"));
        m_wpUsagePowerSeries->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);
        m_wpUsagePowerChart = new QChart();
        m_wpUsagePowerChart->setBackgroundVisible(false);
        m_wpUsagePowerChart->setPlotAreaBackgroundVisible(true);
        m_wpUsagePowerChart->setPlotAreaBackgroundBrush(QColor(8, 27, 58, 210));
        m_wpUsagePowerChart->legend()->setVisible(false);
        m_wpUsagePowerChart->addSeries(m_wpUsagePowerSeries);
        m_wpUsageAxisX_Power = new QBarCategoryAxis(this);
        m_wpUsageAxisX_Power->setLabelsColor(QColor(0xdb, 0xea, 0xfe));
        m_wpUsagePowerChart->addAxis(m_wpUsageAxisX_Power, Qt::AlignBottom);
        m_wpUsagePowerSeries->attachAxis(m_wpUsageAxisX_Power);
        m_wpUsageAxisY_Power = new QValueAxis(this);
        m_wpUsageAxisY_Power->setTitleText(QStringLiteral("用电(Ah)"));
        m_wpUsageAxisY_Power->setLabelsColor(QColor(245, 158, 11));
        m_wpUsageAxisY_Power->setGridLineColor(QColor(125, 211, 252, 35));
        m_wpUsagePowerChart->addAxis(m_wpUsageAxisY_Power, Qt::AlignLeft);
        m_wpUsagePowerSeries->attachAxis(m_wpUsageAxisY_Power);
        m_wpUsagePowerChartView = new QChartView(m_wpUsagePowerChart, chartContainer);
        m_wpUsagePowerChartView->setRenderHint(QPainter::Antialiasing, true);
        m_wpUsagePowerChartView->setMinimumHeight(185);
        m_wpUsagePowerChartView->setMinimumWidth(1000);
        m_wpUsagePowerChartView->setStyleSheet("background:transparent;border:none;");
        chartCLayout->addWidget(m_wpUsagePowerChartView);

        auto* waterChartTitle = new QLabel(QStringLiteral("分时用水"), chartContainer);
        waterChartTitle->setStyleSheet("QLabel{color:#60a5fa;font-size:13px;font-weight:700;padding-top:8px;}");
        chartCLayout->addWidget(waterChartTitle);

        m_wpUsageWaterSet = new QBarSet(QStringLiteral("用水(L)"));
        m_wpUsageWaterSet->setColor(QColor(59, 130, 246));
        m_wpUsageWaterSeries = new QBarSeries(this);
        m_wpUsageWaterSeries->append(m_wpUsageWaterSet);
        m_wpUsageWaterSeries->setLabelsVisible(true);
        m_wpUsageWaterSeries->setLabelsFormat(QStringLiteral("@value"));
        m_wpUsageWaterSeries->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);

        m_wpUsageWaterChart = new QChart();
        m_wpUsageWaterChart->setBackgroundVisible(false);
        m_wpUsageWaterChart->setPlotAreaBackgroundVisible(true);
        m_wpUsageWaterChart->setPlotAreaBackgroundBrush(QColor(8, 27, 58, 210));
        m_wpUsageWaterChart->legend()->setVisible(false);
        m_wpUsageWaterChart->addSeries(m_wpUsageWaterSeries);
        m_wpUsageAxisX_Water = new QBarCategoryAxis(this);
        m_wpUsageAxisX_Water->setLabelsColor(QColor(0xdb, 0xea, 0xfe));
        m_wpUsageWaterChart->addAxis(m_wpUsageAxisX_Water, Qt::AlignBottom);
        m_wpUsageWaterSeries->attachAxis(m_wpUsageAxisX_Water);
        m_wpUsageAxisY_Water = new QValueAxis(this);
        m_wpUsageAxisY_Water->setTitleText(QStringLiteral("用水(L)"));
        m_wpUsageAxisY_Water->setLabelsColor(QColor(59, 130, 246));
        m_wpUsageAxisY_Water->setGridLineColor(QColor(125, 211, 252, 35));
        m_wpUsageWaterChart->addAxis(m_wpUsageAxisY_Water, Qt::AlignLeft);
        m_wpUsageWaterSeries->attachAxis(m_wpUsageAxisY_Water);
        m_wpUsageWaterChartView = new QChartView(m_wpUsageWaterChart, chartContainer);
        m_wpUsageWaterChartView->setRenderHint(QPainter::Antialiasing, true);
        m_wpUsageWaterChartView->setMinimumHeight(185);
        m_wpUsageWaterChartView->setMinimumWidth(1000);
        m_wpUsageWaterChartView->setStyleSheet("background:transparent;border:none;");
        chartCLayout->addWidget(m_wpUsageWaterChartView);

        chartScroll->setWidget(chartContainer);
        usageLayout->addWidget(chartScroll);
        analysisRow->addWidget(usageCard, 1);
    }

    {
        auto* trendCard = createPanelCard(contentWidget);
        auto* trendLayout = new QVBoxLayout(trendCard);
        trendLayout->setContentsMargins(18, 14, 18, 14);
        trendLayout->setSpacing(10);

        auto* trendTitle = new QLabel(QStringLiteral("多日趋势分析"), trendCard);
        trendTitle->setStyleSheet("QLabel{color:#7dd3fc;font-size:15px;font-weight:700;}");
        trendLayout->addWidget(trendTitle);

        auto* trendToolLayout = new QHBoxLayout();
        trendToolLayout->setSpacing(10);
        auto* trendDaysLabel = new QLabel(QStringLiteral("统计区间"), trendCard);
        trendDaysLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
        m_wpTrendDaysCombo = new QComboBox(trendCard);
        m_wpTrendDaysCombo->addItems({QStringLiteral("7 天"), QStringLiteral("15 天"), QStringLiteral("30 天")});
        m_wpTrendDaysCombo->setCurrentIndex(0);
        m_wpTrendDaysCombo->setStyleSheet(analysisControlStyle);
        trendToolLayout->addWidget(trendDaysLabel);
        trendToolLayout->addWidget(m_wpTrendDaysCombo);
        trendToolLayout->addStretch();
        trendLayout->addLayout(trendToolLayout);

        m_wpTrendStatsLabel = new QLabel(QStringLiteral("--"), trendCard);
        m_wpTrendStatsLabel->setWordWrap(true);
        m_wpTrendStatsLabel->setStyleSheet("QLabel{color:#e8f1ff;font-size:13px;line-height:1.9;padding:4px 0 10px 0;}");
        trendLayout->addWidget(m_wpTrendStatsLabel);

        // 趋势图表横向滚动区域
        auto* trendScroll = new QScrollArea(trendCard);
        trendScroll->setWidgetResizable(true);
        trendScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        trendScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        trendScroll->setFixedHeight(470);
        trendScroll->setStyleSheet(
            "QScrollArea{background:transparent;border:none;}"
            "QScrollArea>QWidget>QWidget{background:transparent;}"
            "QScrollBar:horizontal{background:#081b3a;height:8px;border-radius:4px;}"
            "QScrollBar::handle:horizontal{background:#1e4a7a;border-radius:4px;min-width:50px;}"
            "QScrollBar::add-line:horizontal,QScrollBar::sub-line:horizontal{width:0px;border:none;}"
            "QScrollBar::add-page:horizontal,QScrollBar::sub-page:horizontal{background:transparent;}");

        auto* trendContainer = new QWidget(trendScroll);
        auto* trendCLayout = new QVBoxLayout(trendContainer);
        trendCLayout->setContentsMargins(0, 0, 0, 0);
        trendCLayout->setSpacing(8);

        auto* trendPowerTitle = new QLabel(QStringLiteral("每日用电趋势"), trendContainer);
        trendPowerTitle->setStyleSheet("QLabel{color:#fbbf24;font-size:13px;font-weight:700;padding-top:4px;}");
        trendCLayout->addWidget(trendPowerTitle);

        m_wpTrendPowerSet = new QBarSet(QStringLiteral("用电(Ah)"));
        m_wpTrendPowerSet->setColor(QColor(245, 158, 11));
        m_wpTrendPowerSeries = new QBarSeries(this);
        m_wpTrendPowerSeries->append(m_wpTrendPowerSet);
        m_wpTrendPowerSeries->setLabelsVisible(true);
        m_wpTrendPowerSeries->setLabelsFormat(QStringLiteral("@value"));
        m_wpTrendPowerSeries->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);

        m_wpTrendPowerChart = new QChart();
        m_wpTrendPowerChart->setBackgroundVisible(false);
        m_wpTrendPowerChart->setPlotAreaBackgroundVisible(true);
        m_wpTrendPowerChart->setPlotAreaBackgroundBrush(QColor(8, 27, 58, 210));
        m_wpTrendPowerChart->legend()->setVisible(false);
        m_wpTrendPowerChart->addSeries(m_wpTrendPowerSeries);
        m_wpTrendAxisX_Power = new QBarCategoryAxis(this);
        m_wpTrendAxisX_Power->setLabelsColor(QColor(0xdb, 0xea, 0xfe));
        m_wpTrendPowerChart->addAxis(m_wpTrendAxisX_Power, Qt::AlignBottom);
        m_wpTrendPowerSeries->attachAxis(m_wpTrendAxisX_Power);
        m_wpTrendAxisY_Power = new QValueAxis(this);
        m_wpTrendAxisY_Power->setTitleText(QStringLiteral("用电(Ah)"));
        m_wpTrendAxisY_Power->setLabelsColor(QColor(245, 158, 11));
        m_wpTrendAxisY_Power->setGridLineColor(QColor(125, 211, 252, 35));
        m_wpTrendPowerChart->addAxis(m_wpTrendAxisY_Power, Qt::AlignLeft);
        m_wpTrendPowerSeries->attachAxis(m_wpTrendAxisY_Power);
        m_wpTrendPowerChartView = new QChartView(m_wpTrendPowerChart, trendContainer);
        m_wpTrendPowerChartView->setRenderHint(QPainter::Antialiasing, true);
        m_wpTrendPowerChartView->setMinimumHeight(185);
        m_wpTrendPowerChartView->setMinimumWidth(1400);
        m_wpTrendPowerChartView->setStyleSheet("background:transparent;border:none;");
        trendCLayout->addWidget(m_wpTrendPowerChartView);

        auto* trendWaterTitle = new QLabel(QStringLiteral("每日用水趋势"), trendContainer);
        trendWaterTitle->setStyleSheet("QLabel{color:#60a5fa;font-size:13px;font-weight:700;padding-top:8px;}");
        trendCLayout->addWidget(trendWaterTitle);

        m_wpTrendWaterSet = new QBarSet(QStringLiteral("用水(L)"));
        m_wpTrendWaterSet->setColor(QColor(59, 130, 246));
        m_wpTrendWaterSeries = new QBarSeries(this);
        m_wpTrendWaterSeries->append(m_wpTrendWaterSet);
        m_wpTrendWaterSeries->setLabelsVisible(true);
        m_wpTrendWaterSeries->setLabelsFormat(QStringLiteral("@value"));
        m_wpTrendWaterSeries->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);

        m_wpTrendWaterChart = new QChart();
        m_wpTrendWaterChart->setBackgroundVisible(false);
        m_wpTrendWaterChart->setPlotAreaBackgroundVisible(true);
        m_wpTrendWaterChart->setPlotAreaBackgroundBrush(QColor(8, 27, 58, 210));
        m_wpTrendWaterChart->legend()->setVisible(false);
        m_wpTrendWaterChart->addSeries(m_wpTrendWaterSeries);
        m_wpTrendAxisX_Water = new QBarCategoryAxis(this);
        m_wpTrendAxisX_Water->setLabelsColor(QColor(0xdb, 0xea, 0xfe));
        m_wpTrendWaterChart->addAxis(m_wpTrendAxisX_Water, Qt::AlignBottom);
        m_wpTrendWaterSeries->attachAxis(m_wpTrendAxisX_Water);
        m_wpTrendAxisY_Water = new QValueAxis(this);
        m_wpTrendAxisY_Water->setTitleText(QStringLiteral("用水(L)"));
        m_wpTrendAxisY_Water->setLabelsColor(QColor(59, 130, 246));
        m_wpTrendAxisY_Water->setGridLineColor(QColor(125, 211, 252, 35));
        m_wpTrendWaterChart->addAxis(m_wpTrendAxisY_Water, Qt::AlignLeft);
        m_wpTrendWaterSeries->attachAxis(m_wpTrendAxisY_Water);
        m_wpTrendWaterChartView = new QChartView(m_wpTrendWaterChart, trendContainer);
        m_wpTrendWaterChartView->setRenderHint(QPainter::Antialiasing, true);
        m_wpTrendWaterChartView->setMinimumHeight(185);
        m_wpTrendWaterChartView->setMinimumWidth(1400);
        m_wpTrendWaterChartView->setStyleSheet("background:transparent;border:none;");
        trendCLayout->addWidget(m_wpTrendWaterChartView);

        trendScroll->setWidget(trendContainer);
        trendLayout->addWidget(trendScroll);

        analysisRow->addWidget(trendCard, 1);
    }


    pageLayout->addLayout(analysisRow);

    connect(m_wpTrendDaysCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) {
        refreshWaterPowerAnalysisPage();
    });
    connect(m_wpAnalysisDateEdit, &QDateEdit::dateChanged, this, [this](const QDate&) {
        refreshWaterPowerAnalysisPage();
    });

    QTimer::singleShot(100, this, [this]() { refreshWaterPowerAnalysisPage(); });
    scrollArea->setWidget(contentWidget);
    rootLayout->addWidget(scrollArea);
}

void MainWindow::buildHistoryPage() {
    auto* rootLayout = ui->verticalLayoutEnvironment;
    clearLayout(rootLayout);
    rootLayout->setSpacing(12);

    m_historyStartCombo = nullptr;
    m_historyEndCombo = nullptr;
    m_historyMetricChecks.clear();
    qDeleteAll(m_historyLineSeries);
    m_historyLineSeries.clear();
    m_historyChartViews.clear();
    m_historyCharts.clear();
    m_historyAxisXs.clear();
    m_historyAxisYs.clear();
    m_historyChartCards.clear();
    m_historyStatLabels.clear();
    m_historyStatsLayout = nullptr;

    auto* headerCard = createPanelCard(ui->pageEnvironment);
    auto* headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 16, 20, 16);
    auto* titleWrap = new QVBoxLayout();
    auto* titleLabel = new QLabel(QStringLiteral("环境历史"), headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    auto* subLabel = new QLabel(QStringLiteral("针对温度、湿度、PM2.5 与空气指数提供区间趋势和最小值、最大值、均值、最新值统计。"), headerCard);
    subLabel->setWordWrap(true);
    subLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;}");
    titleWrap->addWidget(titleLabel);
    titleWrap->addWidget(subLabel);
    headerLayout->addLayout(titleWrap);
    headerLayout->addStretch();
    auto* exportButton = new QPushButton(QStringLiteral("导出 Excel"), headerCard);
    exportButton->setStyleSheet(
        "QPushButton{background:rgba(18,92,178,0.92);color:white;border:1px solid rgba(125,211,252,0.35);border-radius:10px;padding:8px 18px;font-weight:700;}"
        "QPushButton:hover{background:rgba(26,120,226,0.96);}");
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::onExportHistoryClicked);
    headerLayout->addWidget(exportButton);
    rootLayout->addWidget(headerCard);

    auto* toolCard = createPanelCard(ui->pageEnvironment);
    auto* toolLayout = new QHBoxLayout(toolCard);
    toolLayout->setContentsMargins(16, 10, 16, 10);
    toolLayout->setSpacing(10);
    const QString dtStyle =
        "QDateTimeEdit{background:rgba(7,26,54,0.88);color:#e8f1ff;border:1px solid rgba(96,165,250,0.45);border-radius:8px;padding:6px 10px;font-size:13px;min-width:220px;}"
        "QDateTimeEdit:hover{border-color:rgba(96,165,250,0.7);}"
        "QDateTimeEdit:focus{border-color:#38bdf8;}"
        "QDateTimeEdit::drop-down{subcontrol-origin: padding;subcontrol-position: top right;width:24px;border-left:1px solid rgba(96,165,250,0.35);}"
        "QDateTimeEdit QAbstractItemView{background:#0a1628;color:#e8f1ff;selection-background-color:#0f5fa8;}";
    const QDateTime now = QDateTime::currentDateTime();
    auto* startLabel = new QLabel(QStringLiteral("开始："), toolCard);
    startLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
    m_historyStartCombo = new QDateTimeEdit(now.addDays(-1), toolCard);
    m_historyStartCombo->setCalendarPopup(true);
    m_historyStartCombo->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_historyStartCombo->setMaximumDateTime(now);
    m_historyStartCombo->setStyleSheet(dtStyle);
    m_historyStartCombo->setObjectName("historyStartTime");
    auto* sepLabel = new QLabel(QStringLiteral("~"), toolCard);
    sepLabel->setStyleSheet("QLabel{color:#94a3b8;font-size:14px;}");
    auto* endLabel = new QLabel(QStringLiteral("结束："), toolCard);
    endLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
    m_historyEndCombo = new QDateTimeEdit(now, toolCard);
    m_historyEndCombo->setCalendarPopup(true);
    m_historyEndCombo->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
    m_historyEndCombo->setMaximumDateTime(now);
    m_historyEndCombo->setStyleSheet(dtStyle);
    m_historyEndCombo->setObjectName("historyEndTime");
    auto* sensorLabel = new QLabel(QStringLiteral("指标："), toolCard);
    sensorLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
    auto* metricWrap = new QWidget(toolCard);
    auto* metricLayout = new QHBoxLayout(metricWrap);
    metricLayout->setContentsMargins(0, 0, 0, 0);
    metricLayout->setSpacing(14);
    const QString checkStyle =
        "QCheckBox{color:#e8f1ff;font-size:13px;font-weight:600;spacing:6px;}"
        "QCheckBox::indicator{width:16px;height:16px;border:1px solid rgba(96,165,250,0.55);border-radius:4px;background:rgba(7,26,54,0.88);}"
        "QCheckBox::indicator:checked{background:#0f5fa8;border-color:#38bdf8;}";
    const QStringList metricNames = {
        QStringLiteral("温度"),
        QStringLiteral("湿度"),
        QStringLiteral("PM2.5"),
        QStringLiteral("空气指数")
    };
    for (int i = 0; i < metricNames.size(); ++i) {
        auto* check = new QCheckBox(metricNames[i], metricWrap);
        check->setStyleSheet(checkStyle);
        check->setChecked(i == 0);
        m_historyMetricChecks.append(check);
        metricLayout->addWidget(check);
    }
    metricLayout->addStretch();

    toolLayout->addWidget(startLabel);
    toolLayout->addWidget(m_historyStartCombo);
    toolLayout->addWidget(sepLabel);
    toolLayout->addWidget(endLabel);
    toolLayout->addWidget(m_historyEndCombo);
    toolLayout->addSpacing(8);
    toolLayout->addWidget(sensorLabel);
    toolLayout->addWidget(metricWrap, 1);
    m_historyConfirmBtn = new QPushButton(QStringLiteral("查询历史"), toolCard);
    m_historyConfirmBtn->setStyleSheet(
        "QPushButton{background:rgba(18,92,178,0.92);color:white;border:1px solid rgba(125,211,252,0.35);border-radius:10px;padding:7px 18px;font-weight:700;min-width:110px;}"
        "QPushButton:hover{background:rgba(26,120,226,0.96);}"
        "QPushButton:pressed{background:rgba(14,74,142,0.96);}");
    toolLayout->addWidget(m_historyConfirmBtn);
    rootLayout->addWidget(toolCard);

    auto* chartGrid = new QGridLayout();
    chartGrid->setHorizontalSpacing(12);
    chartGrid->setVerticalSpacing(12);
    const QStringList metricNamesForCards = {
        QStringLiteral("温度趋势"),
        QStringLiteral("湿度趋势"),
        QStringLiteral("PM2.5趋势"),
        QStringLiteral("空气指数趋势")
    };
    for (int i = 0; i < 4; ++i) {
        auto* chartCard = createPanelCard(ui->pageEnvironment);
        auto* cardLayout = new QVBoxLayout(chartCard);
        cardLayout->setContentsMargins(12, 10, 12, 10);
        auto* chartTitle = new QLabel(metricNamesForCards[i], chartCard);
        chartTitle->setStyleSheet("QLabel{font-size:15px;font-weight:700;color:#7dd3fc;}");
        cardLayout->addWidget(chartTitle);

        auto* placeholder = new QLabel(QStringLiteral("请勾选上方指标查看趋势"), chartCard);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet("QLabel{color:#94a3b8;font-size:13px;padding:40px;}");
        placeholder->setMinimumHeight(240);
        cardLayout->addWidget(placeholder);

        m_historyChartCards.append(chartCard);
        m_historyCharts.append(nullptr);
        m_historyAxisXs.append(nullptr);
        m_historyAxisYs.append(nullptr);
        m_historyLineSeries.append(nullptr);
        m_historyChartViews.append(nullptr);

        chartCard->setVisible(i == 0); // 只有温度默认可见
        chartGrid->addWidget(chartCard, i / 2, i % 2);
    }
    rootLayout->addLayout(chartGrid, 1);

    auto* statsCard = createPanelCard(ui->pageEnvironment);
    auto* statsLayout = new QVBoxLayout(statsCard);
    statsLayout->setContentsMargins(18, 14, 18, 14);
    auto* statsTitle = new QLabel(QStringLiteral("数据统计"), statsCard);
    statsTitle->setStyleSheet("QLabel{font-size:15px;font-weight:700;color:#7dd3fc;}");
    statsLayout->addWidget(statsTitle);
    m_historyStatsLayout = new QHBoxLayout();
    m_historyStatsLayout->setSpacing(14);
    statsLayout->addLayout(m_historyStatsLayout);
    for (int i = 0; i < 4; ++i) {
        auto* statLabel = new QLabel(statsCard);
        statLabel->setStyleSheet("QLabel{color:#e8f1ff;font-size:12px;line-height:1.6;background:rgba(7,26,54,0.55);border-radius:6px;padding:10px 14px;}");
        statLabel->setVisible(false);
        m_historyStatLabels.append(statLabel);
        m_historyStatsLayout->addWidget(statLabel);
    }
    m_historyStatsLayout->addStretch();
    rootLayout->addWidget(statsCard);

    if (m_historyPowerStatsLabel) m_historyPowerStatsLabel->deleteLater();
    m_historyPowerStatsLabel = nullptr;
    if (m_historyWaterStatsLabel) m_historyWaterStatsLabel->deleteLater();
    m_historyWaterStatsLabel = nullptr;

    m_historyRefreshTimer = new QTimer(this);
    m_historyRefreshTimer->setSingleShot(true);
    m_historyRefreshTimer->setInterval(250);
    connect(m_historyRefreshTimer, &QTimer::timeout, this, &MainWindow::refreshHistoryPage);

    connect(m_historyConfirmBtn, &QPushButton::clicked, this, [this]() {
        m_historyRefreshTimer->start();
    });
    for (int i = 0; i < m_historyMetricChecks.size(); ++i) {
        QCheckBox* check = m_historyMetricChecks[i];
        const int idx = i;
        connect(check, &QCheckBox::toggled, this, [this, idx](bool checked) {
            if (checked) {
                ensureChartBuilt(idx);
            }
            if (idx < m_historyChartCards.size() && m_historyChartCards[idx]) {
                m_historyChartCards[idx]->setVisible(checked);
            }
            m_historyRefreshTimer->start();
        });
    }

    QTimer::singleShot(100, this, [this]() {
        ensureChartBuilt(0);
        m_historyRefreshTimer->start();
    });
}

void MainWindow::buildAlarmPage() {
    auto* rootLayout = ui->verticalLayoutAlarm;
    clearLayout(rootLayout);
    rootLayout->setSpacing(14);

    auto* headerCard = createPanelCard(ui->pageAlarm);
    auto* headerLayout = new QHBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 16, 20, 16);
    auto* titleWrap = new QVBoxLayout();
    auto* titleLabel = new QLabel(QStringLiteral("报警管理"), headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    auto* subLabel = new QLabel(QStringLiteral("查看与管理历史报警记录，支持按级别筛选。"), headerCard);
    subLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;}");
    titleWrap->addWidget(titleLabel);
    titleWrap->addWidget(subLabel);
    headerLayout->addLayout(titleWrap);
    headerLayout->addStretch();
    rootLayout->addWidget(headerCard);

    auto* statsCard = createPanelCard(ui->pageAlarm);
    auto* statsLayout = new QHBoxLayout(statsCard);
    statsLayout->setContentsMargins(20, 14, 20, 14);
    statsLayout->setSpacing(24);
    auto makeStat = [&](const QString& label, const QString& color, const QString& objName) {
        auto* wrap = new QWidget(statsCard);
        auto* lay = new QVBoxLayout(wrap);
        lay->setContentsMargins(0, 0, 0, 0);
        lay->setSpacing(4);
        auto* val = new QLabel(QStringLiteral("0"), wrap);
        val->setStyleSheet(QString("QLabel{font-size:28px;font-weight:800;color:%1;}").arg(color));
        val->setAlignment(Qt::AlignCenter);
        val->setObjectName(objName);
        auto* lbl = new QLabel(label, wrap);
        lbl->setStyleSheet("QLabel{color:#94a3b8;font-size:12px;}");
        lbl->setAlignment(Qt::AlignCenter);
        lay->addWidget(val);
        lay->addWidget(lbl);
        statsLayout->addWidget(wrap);
    };
    makeStat(QStringLiteral("今日报警"), QStringLiteral("#f8fbff"), QStringLiteral("alarmStatTotal"));
    makeStat(QStringLiteral("严重"), QStringLiteral("#ef4444"), QStringLiteral("alarmStatDanger"));
    makeStat(QStringLiteral("预警"), QStringLiteral("#f59e0b"), QStringLiteral("alarmStatWarn"));
    makeStat(QStringLiteral("求助"), QStringLiteral("#f59e0b"), QStringLiteral("alarmStatHelp"));
    makeStat(QStringLiteral("已解决"), QStringLiteral("#22c55e"), QStringLiteral("alarmStatDone"));
    rootLayout->addWidget(statsCard);

    auto* filterCard = createPanelCard(ui->pageAlarm);
    auto* filterLayout = new QHBoxLayout(filterCard);
    filterLayout->setContentsMargins(16, 10, 16, 10);
    filterLayout->setSpacing(12);
    auto* filterLabel = new QLabel(QStringLiteral("级别筛选："), filterCard);
    filterLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;font-weight:600;}");
    auto* levelFilter = new QComboBox(filterCard);
    levelFilter->addItems({QStringLiteral("全部"), QStringLiteral("严重"), QStringLiteral("预警"), QStringLiteral("求助")});
    levelFilter->setObjectName(QStringLiteral("alarmLevelFilter"));
    levelFilter->setStyleSheet(
        "QComboBox{background:rgba(7,26,54,0.88);color:#e8f1ff;border:1px solid rgba(96,165,250,0.45);border-radius:8px;padding:6px 12px;font-size:13px;min-width:120px;}"
        "QComboBox:hover{border-color:rgba(96,165,250,0.7);}"
        "QComboBox QAbstractItemView{background:#0a1628;color:#e8f1ff;border:1px solid #1e3a5f;selection-background-color:#0f5fa8;outline:none;}");
    filterLayout->addWidget(filterLabel);
    filterLayout->addWidget(levelFilter);
    filterLayout->addStretch();
    rootLayout->addWidget(filterCard);

    m_alarmInfoTable = new QTableWidget(0, 6, ui->pageAlarm);
    m_alarmInfoTable->setHorizontalHeaderLabels({QStringLiteral("起始时间"), QStringLiteral("结束时间"), QStringLiteral("内容"), QStringLiteral("级别"), QStringLiteral("状态"), QStringLiteral("操作")});
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_alarmInfoTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_alarmInfoTable->verticalHeader()->setVisible(false);
    m_alarmInfoTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_alarmInfoTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_alarmInfoTable->setAlternatingRowColors(true);

    auto* infoCard = createPanelCard(ui->pageAlarm);
    auto* infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setContentsMargins(12, 12, 12, 12);
    auto* infoTitle = new QLabel(QStringLiteral("报警记录"), infoCard);
    infoTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
    infoLayout->addWidget(infoTitle);
    infoLayout->addWidget(m_alarmInfoTable);
    rootLayout->addWidget(infoCard, 1);

    connect(levelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        refreshAlarmInfoFromDatabase();
    });

    // 首次进入页面时从数据库加载已有报警记录
    refreshAlarmInfoFromDatabase();
}

#if 0
void MainWindow::buildDevicePage() {
    auto* rootLayout = ui->verticalLayoutWP;
    clearLayout(rootLayout);
    rootLayout->setSpacing(16);

    auto* headerCard = createPanelCard(ui->pageWaterPower);
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(24, 18, 24, 14);
    headerLayout->setSpacing(6);
    auto* titleLabel = new QLabel(QStringLiteral("设备管理"), headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    headerLayout->addWidget(titleLabel);
    auto* hintLabel = new QLabel(QStringLiteral("查看设备状态和远程控制记录。"), headerCard);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("QLabel{color:#94a3b8;font-size:12px;}");
    headerLayout->addWidget(hintLabel);
    rootLayout->addWidget(headerCard);

    auto* infoCard = createPanelCard(ui->pageWaterPower);
    auto* infoLayout = new QVBoxLayout(infoCard);
    infoLayout->setContentsMargins(12, 12, 12, 12);
    auto* infoTitle = new QLabel(QStringLiteral("设备状态"), infoCard);
    infoTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
    infoLayout->addWidget(infoTitle);

    auto* deviceTbl = new QTableWidget(0, 6, infoCard);
    deviceTbl->setObjectName(QStringLiteral("deviceStatusTable"));
    deviceTbl->setHorizontalHeaderLabels({QStringLiteral("ID"), QStringLiteral("名称"), QStringLiteral("类型"), QStringLiteral("状态"), QStringLiteral("实时值"), QStringLiteral("电量")});
    deviceTbl->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    deviceTbl->verticalHeader()->setVisible(false);
    deviceTbl->setEditTriggers(QAbstractItemView::NoEditTriggers);
    deviceTbl->setSelectionBehavior(QAbstractItemView::SelectRows);
    infoLayout->addWidget(deviceTbl);
    rootLayout->addWidget(infoCard, 1);

    m_remoteDeviceCombo = new QComboBox(ui->pageWaterPower);
    m_remoteCommandCombo = new QComboBox(ui->pageWaterPower);
    m_remoteLogMarqueeView = new QPlainTextEdit(ui->pageWaterPower);
    m_remoteLogMarqueeView->setReadOnly(true);
    m_remoteLogMarqueeView->setMinimumHeight(180);
    refreshDeviceTable();
    loadRemoteExecLogTable();
}
#endif

void MainWindow::buildDevicePage() {
    auto* rootLayout = ui->verticalLayoutWP;
    clearLayout(rootLayout);
    rootLayout->setSpacing(16);

    auto* headerCard = createPanelCard(ui->pageWaterPower);
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(24, 18, 24, 14);
    headerLayout->setSpacing(6);
    auto* titleLabel = new QLabel(QStringLiteral("设备管理"), headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    headerLayout->addWidget(titleLabel);
    auto* hintLabel = new QLabel(
        QStringLiteral("上方用于修改阈值，中间用于远程控制，下方用于查看远程控制日志。"),
        headerCard);
    hintLabel->setWordWrap(true);
    hintLabel->setStyleSheet("QLabel{color:#94a3b8;font-size:12px;}");
    headerLayout->addWidget(hintLabel);
    rootLayout->addWidget(headerCard);

    m_remoteDeviceCombo = nullptr;
    m_remoteCommandCombo = nullptr;

    const QString commandTopic = QString::fromLatin1(kMqttCommandPublishTopic);
    auto publishCommand = [this, commandTopic](const QString& commandName,
                                               const QString& payload) -> bool {
        const bool ok = m_mqtt.publishText(commandTopic, payload);
        appendRemoteControlLog(commandName,
                               ok ? QStringLiteral("已发送到 %1").arg(commandTopic)
                                  : QStringLiteral("发送失败：MQTT 未连接"));
        if (!ok) {
            customMessage(this,
                          QStringLiteral("发送失败"),
                          QStringLiteral("MQTT 当前未连接，命令“%1”未发送。").arg(commandName),
                          true);
        }
        return ok;
    };
    {
        struct ThresholdDefinition {
            QString key;
            QString label;
            QString unit;
            int defaultValue;
        };

        const QVector<ThresholdDefinition> thresholdDefs = {
            {QStringLiteral("ta"), QStringLiteral("温度告警上限"), QStringLiteral("℃"), 38},
            {QStringLiteral("tb"), QStringLiteral("温度告警下限"), QStringLiteral("℃"), 10},
            {QStringLiteral("ha"), QStringLiteral("湿度告警上限"), QStringLiteral("%"), 85},
            {QStringLiteral("hb"), QStringLiteral("湿度告警下限"), QStringLiteral("%"), 20},
            {QStringLiteral("pa"), QStringLiteral("PM2.5 告警"), QStringLiteral("ug/m3"), 150},
            {QStringLiteral("aa"), QStringLiteral("空气质量告警"), QString(), 200},
            {QStringLiteral("ca"), QStringLiteral("电流告警"), QStringLiteral("A"), 15},
            {QStringLiteral("fa"), QStringLiteral("水流告警"), QStringLiteral("L/min"), 10},
        };

        // 从持久化存储加载（已被 STM32 遥测 th 同步更新过）
        if (!m_thresholdValues.isEmpty()) {
            // m_thresholdValues 已通过遥测 th + QSettings 初始化，直接用
        }

        auto* thresholdCard = createPanelCard(ui->pageWaterPower);
        auto* thresholdLayout = new QVBoxLayout(thresholdCard);
        thresholdLayout->setContentsMargins(20, 16, 20, 16);
        thresholdLayout->setSpacing(12);

        auto* thresholdTitle = new QLabel(QStringLiteral("修改阈值"), thresholdCard);
        thresholdTitle->setStyleSheet("QLabel{font-size:18px;font-weight:800;color:#7dd3fc;}");
        thresholdLayout->addWidget(thresholdTitle);

        auto* thresholdGrid = new QGridLayout();
        thresholdGrid->setContentsMargins(4, 6, 4, 6);
        thresholdGrid->setHorizontalSpacing(28);
        thresholdGrid->setVerticalSpacing(14);

        QVector<QSpinBox*> thresholdBoxes;
        thresholdBoxes.reserve(thresholdDefs.size());

        auto createSpinBox = [thresholdCard](int value) -> QSpinBox* {
            auto* box = new QSpinBox(thresholdCard);
            box->setRange(0, 5000);
            box->setValue(value);
            box->setFixedSize(128, 38);
            box->setAlignment(Qt::AlignCenter);
            box->setStyleSheet(
                "QSpinBox{background:#0f1f3a;color:#7dd3fc;border:1px solid #2f7fca;"
                "border-radius:8px;padding:0 10px;font-size:14px;font-weight:700;}"
                "QSpinBox:hover{border-color:#5ba0e8;}"
                "QSpinBox:focus{border-color:#38bdf8;background:#0a1630;}"
                "QSpinBox::up-button,QSpinBox::down-button{width:22px;}");
            return box;
        };

        for (int i = 0; i < thresholdDefs.size(); ++i) {
            const auto& def = thresholdDefs[i];
            auto* rowWidget = new QWidget(thresholdCard);
            auto* rowLayout = new QHBoxLayout(rowWidget);
            rowLayout->setContentsMargins(0, 0, 0, 0);
            rowLayout->setSpacing(10);
            rowWidget->setMinimumHeight(44);

            auto* nameLabel = new QLabel(def.label, rowWidget);
            nameLabel->setMinimumWidth(132);
            nameLabel->setStyleSheet("QLabel{color:#e8f0ff;font-size:13px;font-weight:600;}");
            auto* spinBox = createSpinBox(m_thresholdValues.value(def.key, def.defaultValue));
            auto* unitLabel = new QLabel(def.unit, rowWidget);
            unitLabel->setMinimumWidth(44);
            unitLabel->setStyleSheet("QLabel{color:#7a8fb8;font-size:12px;}");

            thresholdBoxes.append(spinBox);
            rowLayout->addWidget(nameLabel);
            rowLayout->addWidget(spinBox);
            rowLayout->addWidget(unitLabel);
            rowLayout->addStretch();
            thresholdGrid->addWidget(rowWidget, i / 2, i % 2);
        }

        thresholdLayout->addLayout(thresholdGrid);

        auto* thresholdButtonRow = new QHBoxLayout();
        thresholdButtonRow->addStretch();

        auto* resetButton = new QPushButton(QStringLiteral("恢复默认"), thresholdCard);
        resetButton->setMinimumHeight(38);
        resetButton->setStyleSheet(
            "QPushButton{background:#1e293b;color:#cbd5e1;border:1px solid #334155;"
            "border-radius:8px;padding:6px 18px;font-size:13px;font-weight:600;}"
            "QPushButton:hover{background:#334155;}");

        auto* sendButton = new QPushButton(QStringLiteral("一键下发"), thresholdCard);
        sendButton->setMinimumHeight(38);
        sendButton->setStyleSheet(
            "QPushButton{background:#0f5fa8;color:white;border:1px solid #2f7fca;"
            "border-radius:8px;padding:6px 24px;font-size:14px;font-weight:700;}"
            "QPushButton:hover{background:#1a7ad4;}"
            "QPushButton:pressed{background:#0a4a8a;}");

        thresholdButtonRow->addWidget(resetButton);
        thresholdButtonRow->addWidget(sendButton);
        thresholdButtonRow->addStretch();
        thresholdLayout->addLayout(thresholdButtonRow);

        connect(resetButton, &QPushButton::clicked, this, [this, publishCommand, thresholdBoxes, thresholdDefs]() {
            for (int i = 0; i < thresholdBoxes.size(); ++i) {
                thresholdBoxes[i]->setValue(thresholdDefs[i].defaultValue);
            }
            if (publishCommand(QStringLiteral("恢复默认阈值"),
                               buildResetThresholdMqttJson())) {
                customMessage(this,
                              QStringLiteral("已发送"),
                              QStringLiteral("恢复默认阈值命令已发送。"));
            }
        });

        connect(sendButton, &QPushButton::clicked, this, [this, publishCommand, thresholdBoxes, thresholdDefs]() {
            if (!customConfirm(this,
                               QStringLiteral("确认下发"),
                               QStringLiteral("将当前阈值下发到现场设备，是否继续？"))) {
                return;
            }

            QJsonObject values;
            for (int i = 0; i < thresholdBoxes.size(); ++i) {
                values.insert(thresholdDefs[i].key, thresholdBoxes[i]->value());
            }

            if (publishCommand(QStringLiteral("阈值一键下发"),
                               buildThresholdMqttJson(values))) {
                // 保存到本地持久化
                for (int i = 0; i < thresholdBoxes.size(); ++i) {
                    m_thresholdValues[thresholdDefs[i].key] = thresholdBoxes[i]->value();
                }
                saveThresholdsToSettings();
                customMessage(this,
                              QStringLiteral("已发送"),
                              QStringLiteral("阈值修改命令已发送到远端设备。"));
            }
        });

        rootLayout->addWidget(thresholdCard);
    }

    {
        auto* controlCard = createPanelCard(ui->pageWaterPower);
        auto* controlLayout = new QVBoxLayout(controlCard);
        controlLayout->setContentsMargins(20, 16, 20, 16);
        controlLayout->setSpacing(12);

        auto* controlTitle = new QLabel(QStringLiteral("远程控制"), controlCard);
        controlTitle->setStyleSheet("QLabel{font-size:18px;font-weight:800;color:#7dd3fc;}");
        controlLayout->addWidget(controlTitle);

        const QString ctrlBtnNormal =
            "QPushButton{background:rgba(37,99,235,0.45);color:#e8f1ff;border:1px solid rgba(96,165,250,0.45);"
            "border-radius:8px;padding:6px 18px;font-weight:600;font-size:13px;}"
            "QPushButton:hover{background:rgba(59,130,246,0.65);}";
        const QString ctrlOnActive =
            "QPushButton{background:#075985;color:#f0f9ff;border:2px solid #38bdf8;border-radius:8px;"
            "padding:5px 16px;font-weight:800;font-size:13px;}"
            "QPushButton:hover{background:#0c4a6e;}";

        auto publishDeviceCode = [publishCommand](int code, const QString& actionName) -> bool {
            QJsonObject payload;
            payload.insert(QStringLiteral("kind"), QStringLiteral("control"));
            payload.insert(QStringLiteral("code"), code);
            payload.insert(QStringLiteral("name"), actionName);
            return publishCommand(actionName,
                                  buildWrappedMqttJson(QStringLiteral("command"),
                                                       QStringLiteral("qt"),
                                                       payload));
        };

        auto createCodeControlRow = [this, controlCard, controlLayout, publishDeviceCode, ctrlBtnNormal, ctrlOnActive](
                                        const QString& title,
                                        const QString& onText,
                                        const QString& offText,
                                        int onCode,
                                        int offCode,
                                        const QString& onLogName,
                                        const QString& offLogName) {
            auto* row = new QHBoxLayout();
            row->setSpacing(12);

            auto* label = new QLabel(title, controlCard);
            label->setMinimumWidth(86);
            label->setStyleSheet("QLabel{font-size:14px;color:#dbeafe;font-weight:700;}");

            auto* onButton = new QPushButton(onText, controlCard);
            auto* offButton = new QPushButton(offText, controlCard);
            onButton->setMinimumHeight(36);
            offButton->setMinimumHeight(36);
            onButton->setMinimumWidth(120);
            offButton->setMinimumWidth(120);
            onButton->setStyleSheet(ctrlBtnNormal);
            offButton->setStyleSheet(ctrlBtnNormal);

            connect(onButton, &QPushButton::clicked, this, [publishDeviceCode, onCode, onLogName, onButton, ctrlOnActive]() {
                if (publishDeviceCode(onCode, onLogName)) {
                    onButton->setStyleSheet(ctrlOnActive);
                }
            });
            connect(offButton, &QPushButton::clicked, this, [publishDeviceCode, offCode, offLogName, onButton, ctrlBtnNormal]() {
                if (publishDeviceCode(offCode, offLogName)) {
                    onButton->setStyleSheet(ctrlBtnNormal);
                }
            });

            row->addWidget(label, 0, Qt::AlignLeft | Qt::AlignVCenter);
            row->addStretch();
            row->addWidget(onButton, 0, Qt::AlignRight);
            row->addWidget(offButton, 0, Qt::AlignRight);
            controlLayout->addLayout(row);
        };

        createCodeControlRow(QStringLiteral("警报"),
                             QStringLiteral("开启"),
                             QStringLiteral("关闭"),
                             1,
                             0,
                             QStringLiteral("警报开启"),
                             QStringLiteral("警报关闭"));
        createCodeControlRow(QStringLiteral("风扇"),
                             QStringLiteral("开启"),
                             QStringLiteral("关闭"),
                             4,
                             5,
                             QStringLiteral("风扇开启"),
                             QStringLiteral("风扇关闭"));
        createCodeControlRow(QStringLiteral("窗户"),
                             QStringLiteral("打开"),
                             QStringLiteral("关闭"),
                             2,
                             3,
                             QStringLiteral("窗户打开"),
                             QStringLiteral("窗户关闭"));
        createCodeControlRow(QStringLiteral("警报灯"),
                             QStringLiteral("开启"),
                             QStringLiteral("关闭"),
                             6,
                             7,
                             QStringLiteral("警报灯开启"),
                             QStringLiteral("警报灯关闭"));

        rootLayout->addWidget(controlCard);
    }

    {
        auto* logCard = createPanelCard(ui->pageWaterPower);
        auto* logLayout = new QVBoxLayout(logCard);
        logLayout->setContentsMargins(16, 14, 16, 14);
        logLayout->setSpacing(10);

        auto* logTitle = new QLabel(QStringLiteral("远程控制日志"), logCard);
        logTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
        logLayout->addWidget(logTitle);

        m_remoteLogTable = new QTableWidget(0, 3, logCard);
        m_remoteLogTable->setHorizontalHeaderLabels({
            QStringLiteral("时间"), QStringLiteral("控制命令"), QStringLiteral("执行结果")});
        m_remoteLogTable->setMinimumHeight(260);
        m_remoteLogTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_remoteLogTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_remoteLogTable->setSelectionMode(QAbstractItemView::SingleSelection);
        m_remoteLogTable->verticalHeader()->setVisible(false);
        m_remoteLogTable->setShowGrid(false);
        m_remoteLogTable->setAlternatingRowColors(true);

        // 列宽：时间固定、命令自适应、结果固定
        m_remoteLogTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
        m_remoteLogTable->setColumnWidth(0, 170);   // 时间
        m_remoteLogTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);  // 控制命令
        m_remoteLogTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
        m_remoteLogTable->setColumnWidth(2, 220);   // 执行结果

        m_remoteLogTable->setStyleSheet(
            "QTableWidget{background:rgba(7,26,54,0.86);border:1px solid rgba(96,165,250,0.30);"
            "border-radius:10px;color:#dbeafe;font-size:13px;}"
            "QTableWidget::item{padding:6px 8px;}"
            "QHeaderView::section{"
            "background:rgba(15,95,168,0.55);color:#bfdbfe;font-size:12px;font-weight:700;"
            "padding:8px 10px;border:none;border-right:1px solid rgba(96,165,250,0.15);}"
            "QTableWidget::item:alternate{background:rgba(15,95,168,0.08);}");

        logLayout->addWidget(m_remoteLogTable, 1);
        rootLayout->addWidget(logCard, 1);
    }

    loadRemoteExecLogTable();
}

void MainWindow::buildSettingsPage() {
    auto* rootLayout = ui->verticalLayoutSetting;
    clearLayout(rootLayout);
    rootLayout->setSpacing(14);

    auto* headerCard = createPanelCard(ui->pageSetting);
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(20, 16, 20, 16);
    auto* titleLabel = new QLabel(QStringLiteral("系统设置"), headerCard);
    titleLabel->setStyleSheet("QLabel{font-size:24px;font-weight:800;color:#7dd3fc;}");
    auto* subLabel = new QLabel(QStringLiteral("账号信息、连接状态与系统日志。"), headerCard);
    subLabel->setStyleSheet("QLabel{color:#bfdcff;font-size:13px;}");
    headerLayout->addWidget(titleLabel);
    headerLayout->addWidget(subLabel);
    rootLayout->addWidget(headerCard);

    auto* accountCard = createPanelCard(ui->pageSetting);
    auto* accountLayout = new QGridLayout(accountCard);
    accountLayout->setContentsMargins(18, 16, 18, 16);
    accountLayout->setHorizontalSpacing(12);
    accountLayout->setVerticalSpacing(10);
    auto* accountTitle = new QLabel(QStringLiteral("当前账号信息"), accountCard);
    accountTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
    accountLayout->addWidget(accountTitle, 0, 0, 1, 3);

    const QString roleText = (m_userRole == QStringLiteral("admin")) ? QStringLiteral("管理员") : QStringLiteral("普通用户");
    accountLayout->addWidget(new QLabel(QStringLiteral("账号："), accountCard), 1, 0);
    accountLayout->addWidget(new QLabel(m_userName, accountCard), 1, 1);
    accountLayout->addWidget(new QLabel(QStringLiteral("角色："), accountCard), 2, 0);
    accountLayout->addWidget(new QLabel(roleText, accountCard), 2, 1);
    accountLayout->addWidget(new QLabel(QStringLiteral("登录时间："), accountCard), 3, 0);
    accountLayout->addWidget(new QLabel(m_loginTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), accountCard), 3, 1);

    auto* editAccountButton = new QPushButton(QStringLiteral("修改信息"), accountCard);
    connect(editAccountButton, &QPushButton::clicked, this, &MainWindow::onEditAccountInfoClicked);
    accountLayout->addWidget(editAccountButton, 5, 0, 1, 1, Qt::AlignLeft);

    auto* addAccountButton = new QPushButton(QStringLiteral("添加账户"), accountCard);
    connect(addAccountButton, &QPushButton::clicked, this, &MainWindow::onAddAccountClicked);
    accountLayout->addWidget(addAccountButton, 5, 1, 1, 1, Qt::AlignCenter);

    auto* switchAccountButton = new QPushButton(QStringLiteral("切换账号"), accountCard);
    connect(switchAccountButton, &QPushButton::clicked, this, &MainWindow::onSwitchAccountClicked);
    accountLayout->addWidget(switchAccountButton, 5, 2, 1, 1, Qt::AlignRight);
    rootLayout->addWidget(accountCard);

    auto* statusCard = createPanelCard(ui->pageSetting);
    auto* statusRow = new QHBoxLayout(statusCard);
    statusRow->setContentsMargins(18, 16, 18, 16);
    statusRow->setSpacing(16);

    auto addInfoRow = [](QVBoxLayout* lay, const QString& label, const QString& value, const QString& color = QStringLiteral("#e8f1ff")) {
        auto* row = new QHBoxLayout();
        auto* lbl = new QLabel(label);
        lbl->setStyleSheet("QLabel{color:#94a3b8;font-size:12px;}");
        auto* val = new QLabel(value);
        val->setStyleSheet(QString("QLabel{color:%1;font-size:12px;font-weight:600;}").arg(color));
        val->setWordWrap(true);
        row->addWidget(lbl);
        row->addWidget(val, 1);
        lay->addLayout(row);
    };

    auto* mqttBox = new QWidget(statusCard);
    auto* mqttLay = new QVBoxLayout(mqttBox);
    auto* mqttTitle = new QLabel(QStringLiteral("MQTT 连接"), mqttBox);
    mqttTitle->setStyleSheet("QLabel{font-size:15px;font-weight:700;color:#7dd3fc;}");
    mqttLay->addWidget(mqttTitle);
    const bool mqttConnected = m_mqtt.isConnected();
    addInfoRow(mqttLay, QStringLiteral("状态："), mqttConnected ? QStringLiteral("已连接") : QStringLiteral("未连接"), mqttConnected ? QStringLiteral("#22c55e") : QStringLiteral("#ef4444"));
    addInfoRow(mqttLay, QStringLiteral("Broker："), QStringLiteral("bemfa.com:9501"));
    addInfoRow(mqttLay, QStringLiteral("Topic："), QStringLiteral("telemetry / command / help"));
    mqttLay->addStretch();

    auto* dbBox = new QWidget(statusCard);
    auto* dbLay = new QVBoxLayout(dbBox);
    auto* dbTitle = new QLabel(QStringLiteral("数据库信息"), dbBox);
    dbTitle->setStyleSheet("QLabel{font-size:15px;font-weight:700;color:#7dd3fc;}");
    dbLay->addWidget(dbTitle);
    const QString dbPath = QDir(DbConfig::kDbDir).filePath(DbConfig::kDbFileName);
    QFileInfo dbFi(dbPath);
    addInfoRow(dbLay, QStringLiteral("路径："), QDir::toNativeSeparators(dbPath));
    addInfoRow(dbLay, QStringLiteral("大小："), dbFi.exists() ? QStringLiteral("%1 KB").arg(dbFi.size() / 1024) : QStringLiteral("尚未创建"));
    addInfoRow(dbLay, QStringLiteral("数据库："), m_db ? QStringLiteral("已连接") : QStringLiteral("未连接"), m_db ? QStringLiteral("#22c55e") : QStringLiteral("#f59e0b"));
    addInfoRow(dbLay, QStringLiteral("最后更新："), dbFi.exists() ? dbFi.lastModified().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("--"));
    dbLay->addStretch();

    statusRow->addWidget(mqttBox);
    statusRow->addWidget(dbBox);
    rootLayout->addWidget(statusCard);

    m_logViewer = new QPlainTextEdit(ui->pageSetting);
    m_logViewer->setReadOnly(true);
    m_logViewer->setPlainText(QStringLiteral("%1 %2 登录").arg(m_loginTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")), m_userName));

    auto* logCard = createPanelCard(ui->pageSetting);
    auto* logLayout = new QVBoxLayout(logCard);
    logLayout->setContentsMargins(12, 12, 12, 12);
    auto* logTitle = new QLabel(QStringLiteral("系统日志"), logCard);
    logTitle->setStyleSheet("QLabel{font-size:16px;font-weight:700;color:#7dd3fc;}");
    logLayout->addWidget(logTitle);
    logLayout->addWidget(m_logViewer);
    rootLayout->addWidget(logCard, 1);
}
