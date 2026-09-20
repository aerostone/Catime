/**
 * @file todo_date_range.h
 * @brief Calendar window helpers shared by the list dialog and boards.
 *
 * Week = Monday..Sunday, Month = 1st..last day, both of the current
 * local date. Pure date math on SYSTEMTIME/FILETIME.
 */
#ifndef CATIME_TODO_DATE_RANGE_H
#define CATIME_TODO_DATE_RANGE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void TodoDateRange_CurrentWeek(char *from, size_t fromCap,
                              char *to, size_t toCap);
void TodoDateRange_CurrentMonth(char *from, size_t fromCap,
                               char *to, size_t toCap);

#ifdef __cplusplus
}
#endif

#endif /* CATIME_TODO_DATE_RANGE_H */
