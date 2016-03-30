#include "TestMainWindow.h"
#include "gui/intervalProgress/IntervalProgressDisplay.h"
#include <QPushButton>

    TestMainWindow::TestMainWindow()
        :QFrame(),
        currentBeat(0)
    {
        refreshTimerID = startTimer(500);

        setMinimumSize(180, 180);

        progressDisplay = new IntervalProgressDisplay(this);
        progressDisplay->setBeatsPerAccent(4);
        progressDisplay->setShowAccents(false);

        QVBoxLayout *mainLayout = new QVBoxLayout();
        mainLayout->addWidget(progressDisplay, 1);
        this->setLayout(mainLayout);

        QComboBox *combo = new QComboBox();
        combo->addItem("Line");
        combo->addItem("Circle");
        combo->addItem("Ellipse");

        QPushButton *accentsButton = new QPushButton("Accents");
        accentsButton->setCheckable(true);

        QHBoxLayout *controlsLayout = new QHBoxLayout();
        controlsLayout->addWidget(combo);
        controlsLayout->addWidget(accentsButton);

        mainLayout->addLayout(controlsLayout);

        connect(combo, SIGNAL(currentIndexChanged(QString)), this, SLOT(setShape(QString)));
        connect(accentsButton, SIGNAL(toggled(bool)), this, SLOT(toggleAccents(bool)));

        progressDisplay->setBeatsPerInterval(8);
        currentBeat = progressDisplay->getBeatsPerInterval()/2 - 1;
        combo->setCurrentIndex(1);
    }

    TestMainWindow::~TestMainWindow()
    {
        killTimer(refreshTimerID);
    }

    void TestMainWindow::timerEvent(QTimerEvent *){
        int beatsPerInterval = progressDisplay->getBeatsPerInterval();
        currentBeat = (currentBeat + 1) % beatsPerInterval;
        progressDisplay->setCurrentBeat(currentBeat);
        update();
    }

    void TestMainWindow::toggleAccents(bool showAccents)
    {
        progressDisplay->setShowAccents(showAccents);
    }

    void TestMainWindow::setShape(QString shape)
    {
        if ("Circle" == shape){
            progressDisplay->setPaintMode(IntervalProgressDisplay::CIRCULAR);
        }
        else if ("Line" == shape) {
            progressDisplay->setPaintMode(IntervalProgressDisplay::LINEAR);
        }
        else{
            progressDisplay->setPaintMode(IntervalProgressDisplay::ELLIPTICAL);
        }
        update();
    }


