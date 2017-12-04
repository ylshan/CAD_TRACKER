#include"tracer.h"
/*
矩阵相乘
pragma one:内参矩阵
pragma two:相机坐标系采样点集
pragma three:前一帧姿态矩阵
pragma four: CAD模型可见采样点
pragma five: 采样点和搜索点相对位置误差
pragma six:采样的个数
pragma seven:法向量
*/

//更新位姿
static int update_gesture(MARTIX six_freedom, MARTIX curE, MARTIX* nxtE,MARTIX changeGesture)
{
    int ret = 0;
    if (six_freedom.martix[0] < 3 && six_freedom.martix[1] < 3 && six_freedom.martix[2] < 3)
    {
        mul_maritx(curE, changeGesture, nxtE);
    }
    else
    {
        assign_martix(curE, nxtE);
    }
    return ret;
}

//根据六个自由度计算姿态变化矩阵M
static int gesture_change(MARTIX six_freedom,MARTIX* lds,MARTIX* gestureChangeM)
{
    int ret = 0;
    if (!lds || !gestureChangeM)
    {
        ret = -1;
        printf("李代数矩阵或者姿态变化矩阵不能为空");
        return ret;
    }

    gestureChangeM->cols = gestureChangeM->rows = 4;

    //构造一个4*4的单位矩阵
    MARTIX singleMartix;
    singleMartix.cols = singleMartix.rows = 4;
    singleMartix.martix = (float*)malloc(sizeof(float)*pow(4, 2));
    memset(singleMartix.martix, 0, sizeof(float)*pow(4, 2));
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (i == j)
            {
                singleMartix.martix[i * 4 + j] = 1;
            }
            else {
                singleMartix.martix[i * 4 + j] = 0;
            }
        }
    }

    //泰勒展开的后四项
    MARTIX tempMartix[4];
    for (int i = 0; i < 4; i++)
    {
        tempMartix[i].cols = 4;
        tempMartix[i].rows = 4;
        tempMartix[i].martix = (float*)malloc(sizeof(float)*pow(4, 2));
        memset(tempMartix[i].martix, 0, sizeof(float)*pow(4, 2));
    }
    
    
    for (int j = 0; j < 6; j++)
    {
        num_mul_matrix(lds[j], six_freedom.martix[j], &lds[j]);
        add_maritx(tempMartix[0], lds[j], &tempMartix[0]);
    }


    mul_maritx(tempMartix[0], tempMartix[0], &tempMartix[1]);
    num_mul_matrix(tempMartix[1],1/2, &tempMartix[1]);

    mul_maritx(tempMartix[1], tempMartix[0], &tempMartix[2]);
    num_mul_matrix(tempMartix[2], 1/6, &tempMartix[2]);

    mul_maritx(tempMartix[1], tempMartix[1], &tempMartix[3]);
    num_mul_matrix(tempMartix[3], 1 / 24, &tempMartix[3]);

    add_maritx(singleMartix, tempMartix[0], gestureChangeM);
    for (int i = 1; i < 4; i++)
    {
        add_maritx(*gestureChangeM, tempMartix[i], gestureChangeM);
    }
    return ret;
}



//计算六个自由度u1,u2,u3,u4,u5,u6
static int six_freedom(MARTIX J_martix, MARTIX W_martix, MARTIX E_martix, MARTIX *result_martix)
{
    int ret = 0;
    MARTIX trs_J_martix;
    MARTIX temp_martix1;
    MARTIX temp_martix2;
    MARTIX temp_converse_martix;

    trs_J_martix.cols = J_martix.rows;
    trs_J_martix.rows = J_martix.cols;
    trs_J_martix.martix = (float*)malloc(sizeof(float)*trs_J_martix.cols*trs_J_martix.rows);
    ret = translate_martix(J_martix, &trs_J_martix);

    temp_martix1.rows = 6;
    temp_martix1.cols = J_martix.cols;
    temp_martix1.martix = (float*)malloc(sizeof(float)*temp_martix1.cols*temp_martix1.rows);
    ret = mul_maritx(trs_J_martix, W_martix, &temp_martix1);

    temp_martix2.rows = 6;
    temp_martix2.cols = 6;
    temp_martix2.martix = (float*)malloc(sizeof(float)*temp_martix2.cols*temp_martix2.rows);
    ret = mul_maritx(temp_martix1, J_martix, &temp_martix2);

    temp_converse_martix.cols = 6;
    temp_converse_martix.rows = 6;
    temp_converse_martix.martix = (float*)malloc(sizeof(float)*temp_converse_martix.cols*temp_converse_martix.rows);
    ret = converse_martix(temp_martix2, &temp_converse_martix);

    ret = mul_maritx(temp_converse_martix, trs_J_martix, &temp_martix1);
    ret = mul_maritx(temp_martix1, W_martix, &temp_martix1);

    result_martix->rows = 6;
    result_martix->cols = 1;
    //    result_martix->martix = (float*)malloc(sizeof(float)*result_martix->rows*result_martix->cols);
    ret = mul_maritx(temp_martix1, E_martix, result_martix);

    if (trs_J_martix.martix)
    {
        free(trs_J_martix.martix);
        trs_J_martix.martix = NULL;
    }
    if (temp_martix1.martix)
    {
        free(temp_martix1.martix);
        temp_martix1.martix = NULL;
    }
    if (temp_martix2.martix)
    {
        free(temp_martix2.martix);
        temp_martix2.martix = NULL;
    }
    if (temp_converse_martix.martix)
    {
        free(temp_converse_martix.martix);
        temp_converse_martix.martix = NULL;
    }
    return ret;
}

//求加权矩阵W,W对应的是一个对角矩阵
static int weight_error(int gesture_nnum, float* randomError,MARTIX* weight_martix)
{
    int ret = 0;
    if (!randomError || !weight_martix)
    {
        ret = -1;
        printf("误差矩阵或者权重矩阵不能为空");
        return ret;
    }
    weight_martix->cols = weight_martix->rows = gesture_nnum;
    for (int i = 0; i < gesture_nnum; i++)
    {
        weight_martix->martix[i*gesture_nnum + i] = 1 / (0.01 + randomError[i]);
    }
    return ret;
}



//最小二乘获取新位姿
int leastSquares(MARTIX internalRef, threespace* CameraSamplePoint, MARTIX gesture, 
    threespace *ModulePoint, float* randomError, int gesture_nnum,twospace* deviation, MARTIX* nxtGesture)
{
    int ret = 0;
    if (!CameraSamplePoint || !ModulePoint)
    {
        ret = -1;
        printf("相机坐标系采样点或CAD模型采样点不能为空");
        return ret;
    }

    //根据相机内参构造 2*2 JK矩阵
    MARTIX JK;
    JK.cols = JK.rows = 2;
    JK.martix = (float*)malloc(sizeof(float)*JK.cols*JK.rows);
    JK.martix[0] = internalRef.martix[0];
    JK.martix[1] = internalRef.martix[1];
    JK.martix[2] = internalRef.martix[3];
    JK.martix[3] = internalRef.martix[4];

    //误差矩阵
    MARTIX E_martix;
    E_martix.rows = gesture_nnum;
    E_martix.cols = 1;
    E_martix.martix = (float*)malloc(sizeof(float)*E_martix.rows*E_martix.cols);

    //构造6个李代数旋转矩阵
    MARTIX G[6];
    G[1].rows = G[2].rows = G[3].rows = G[4].rows = G[5].rows = G[6].rows = 4;
    G[1].cols = G[2].cols = G[3].cols = G[4].cols = G[5].cols = G[6].cols = 4;
    G[1].martix = (float*)malloc(sizeof(float)*G[1].rows*G[1].cols);
    memset(G[1].martix, 0, sizeof(float)*G[1].rows*G[1].cols);
    G[1].martix[3] = 1;

    G[2].martix = (float*)malloc(sizeof(float)*G[2].rows*G[2].cols);
    memset(G[2].martix, 0, sizeof(float)*G[2].rows*G[2].cols);
    G[2].martix[7] = 1;

    G[3].martix = (float*)malloc(sizeof(float)*G[3].rows*G[3].cols);
    memset(G[3].martix, 0, sizeof(float)*G[3].rows*G[3].cols);
    G[3].martix[11] = 1;

    G[4].martix = (float*)malloc(sizeof(float)*G[4].rows*G[4].cols);
    memset(G[4].martix, 0, sizeof(float)*G[4].rows*G[4].cols);
    G[4].martix[6] = -1;
    G[4].martix[9] = 1;

    G[5].martix = (float*)malloc(sizeof(float)*G[5].rows*G[5].cols);
    memset(G[5].martix, 0, sizeof(float)*G[5].rows*G[5].cols);
    G[5].martix[2] = 1;
    G[5].martix[8] = -1;

    G[6].martix = (float*)malloc(sizeof(float)*G[6].rows*G[6].cols);
    memset(G[6].martix, 0, sizeof(float)*G[6].rows*G[6].cols);
    G[6].martix[1] = -1;
    G[6].martix[4] = 1;

    //缓存雅克比矩阵Jij
    MARTIX Jij;
    Jij.cols = 6;
    Jij.rows = gesture_nnum;
    Jij.martix = (float*)malloc(sizeof(float)*Jij.rows*Jij.cols);

    //构造一个n*6的雅克比矩阵(n代表采样点的个数)
    for (int i = 0; i < gesture_nnum; i++)
    {
        //根据相机坐标系下面的坐标点构造JP矩阵
        MARTIX JP;
        JP.cols = 4;
        JP.rows = 2;
        JP.martix = (float*)malloc(sizeof(float)*JP.cols*JP.rows);
        JP.martix[0] = 1 / CameraSamplePoint[i].real_z;
        JP.martix[1] = 0.0f;
        JP.martix[2] = -(CameraSamplePoint[i].real_x) / (pow(CameraSamplePoint[i].real_z, 2));
        JP.martix[3] = 0.0f;
        JP.martix[4] = 0.0f;
        JP.martix[5] = 1 / CameraSamplePoint[i].real_z;
        JP.martix[6] = -(CameraSamplePoint[i].real_y) / (pow(CameraSamplePoint[i].real_z, 2));
        JP.martix[7] = 0;

        //构建CAD模型可见的采集点矩阵Pi
        MARTIX Pi;
        Pi.cols = 1;
        Pi.rows = 4;
        Pi.martix = (float*)malloc(sizeof(float)*Pi.cols*Pi.rows);
        Pi.martix[0] = ModulePoint[i].real_x;
        Pi.martix[1] = ModulePoint[i].real_y;
        Pi.martix[2] = ModulePoint[i].real_z;
        Pi.martix[3] = 1;

        //赋值误差矩阵
        E_martix.martix[i] = randomError[i];

        for (int j = 0; j < 6; j++)
        {
            //niT矩阵(1*2)
            MARTIX trs_ni;
            trs_ni.cols = 2;
            trs_ni.rows = 1;
            trs_ni.martix = (float*)malloc(sizeof(float)*trs_ni.cols*trs_ni.rows);
            trs_ni.martix[0] = deviation[i].real_x;
            trs_ni.martix[1] = deviation[i].real_y;
            
            //niT*Jk缓存矩阵
            MARTIX output_martix1;
            output_martix1.rows = 1;
            output_martix1.cols = 2;
            output_martix1.martix = (float*)malloc(sizeof(float)*output_martix1.rows*output_martix1.cols);
          
            //niT*Jk
            ret = mul_maritx(trs_ni, JK, &output_martix1);

            //niT*JK*[Jp 0]
            //niT*Jk*[Jp 0]*Et缓存矩阵
            MARTIX output_martix2;
            output_martix2.rows = 1;
            output_martix2.cols = 4;
            output_martix2.martix = (float*)malloc(sizeof(float)*output_martix2.rows*output_martix2.cols);
            ret = mul_maritx(output_martix1, JP, &output_martix2);
            ret= mul_maritx(output_martix2, gesture, &output_martix2);

            //Gj*Pi的缓存矩阵
            MARTIX output_martix3;
            output_martix3.rows = 4;
            output_martix3.cols = 1;
            output_martix3.martix = (float*)malloc(sizeof(float)*output_martix3.rows*output_martix3.cols);
            ret = mul_maritx(G[j], Pi, &output_martix3);

            //Jij结果矩阵1*1
            MARTIX result_martix;
            result_martix.rows = 1;
            result_martix.cols = 1;
            result_martix.martix = (float*)malloc(sizeof(float)*result_martix.rows*result_martix.cols);
            ret = mul_maritx(output_martix2, output_martix3, &result_martix);

            Jij.martix[i * 6 + j] = result_martix.martix[0];


            if (trs_ni.martix)
            {
                free(trs_ni.martix);
                trs_ni.martix = NULL;
            }
            if (output_martix1.martix)
            {
                free(output_martix1.martix);
                output_martix1.martix = NULL;
            }
            if (output_martix2.martix)
            {
                free(output_martix2.martix);
                output_martix2.martix = NULL;
            }
            if (output_martix3.martix)
            {
                free(output_martix3.martix);
                output_martix3.martix = NULL;
            }
            if (result_martix.martix)
            {
                free(result_martix.martix);
                result_martix.martix = NULL;
            }

        }

        if (JP.martix)
        {
            free(JP.martix);
            JP.martix = NULL;
        }
        if (Pi.martix)
        {
            free(Pi.martix);
            Pi.martix = NULL;
        }
    }

    //计算加权矩阵W
    MARTIX weight_martix;
    weight_martix.cols = gesture_nnum;
    weight_martix.rows = gesture_nnum;
    weight_martix.martix = (float*)malloc(sizeof(float)*pow(gesture_nnum, 2));
    memset(weight_martix.martix, 0, sizeof(float)*pow(gesture_nnum, 2));
 
    ret = weight_error(gesture_nnum, randomError, &weight_martix);

    //缓存六个自由度
    MARTIX sixFreedom;
    sixFreedom.cols = 1;
    sixFreedom.rows = 6;
    sixFreedom.martix = (float*)malloc(sizeof(float)*sixFreedom.cols*sixFreedom.rows);
    //计算六个自由度
    ret = six_freedom(Jij, weight_martix, E_martix, &sixFreedom);

    //姿态变化矩阵
    MARTIX gestureChangeM;
    gestureChangeM.cols = gestureChangeM.rows = 4;
    gestureChangeM.martix = (float*)malloc(sizeof(float)*pow(4, 2));
    memset(gestureChangeM.martix, 0, sizeof(float)*pow(4, 2));

    //根据自由度计算姿态变化矩阵M
    ret = gesture_change(sixFreedom, G, &gestureChangeM);
    
    //更新位姿
    ret = update_gesture(sixFreedom, gesture, nxtGesture, gestureChangeM);


    //释放空间
    if (JK.martix)
    {
        free(JK.martix);
        JK.martix = NULL;
    }

    for (int i = 0; i < 6; i++)
    {
        if (G[i].martix)
        {
            free(G[i].martix);
            G[i].martix = NULL;
        }
    }
    if (weight_martix.martix)
    {
        free(weight_martix.martix);
        weight_martix.martix = NULL;
    }
    if (sixFreedom.martix)
    {
        free(sixFreedom.martix);
        sixFreedom.martix = NULL;
    }
    return ret;
}